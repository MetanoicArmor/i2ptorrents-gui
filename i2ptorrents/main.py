from __future__ import annotations

import shutil
import sys
from pathlib import Path
from typing import Any, Callable

from PyQt6 import QtCore, QtGui, QtWidgets

from .models import Torrent, TorrentStatus, format_bytes, format_rate
from .rpc import RPCError, TransmissionRPC, normalize_rpc_url
from .settings import AppSettings
from .theme import stylesheet


class WorkerSignals(QtCore.QObject):
    succeeded = QtCore.pyqtSignal(object)
    failed = QtCore.pyqtSignal(str)


class Worker(QtCore.QRunnable):
    def __init__(self, call: Callable[[], Any]) -> None:
        super().__init__()
        self.call = call
        self.signals = WorkerSignals()

    @QtCore.pyqtSlot()
    def run(self) -> None:
        try:
            self.signals.succeeded.emit(self.call())
        except Exception as exc:
            self.signals.failed.emit(str(exc))


class TorrentCard(QtWidgets.QFrame):
    removeRequested = QtCore.pyqtSignal(int, str)

    def __init__(self, torrent: Torrent, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("TorrentCard")
        self.setProperty("torrentId", torrent.id)
        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(16, 14, 16, 14)
        root.setSpacing(9)

        title_row = QtWidgets.QHBoxLayout()
        name = QtWidgets.QLabel(torrent.name)
        name.setObjectName("TorrentName")
        name.setTextInteractionFlags(QtCore.Qt.TextInteractionFlag.TextSelectableByMouse)
        title_row.addWidget(name, 1)
        status = QtWidgets.QLabel(torrent.status.label)
        status.setObjectName("StatusText")
        title_row.addWidget(status)
        more = QtWidgets.QToolButton()
        more.setText("•••")
        more.setToolTip("Действия")
        menu = QtWidgets.QMenu(more)
        remove = menu.addAction("Удалить из списка…")
        remove.triggered.connect(lambda: self.removeRequested.emit(torrent.id, torrent.name))
        more.setMenu(menu)
        more.setPopupMode(QtWidgets.QToolButton.ToolButtonPopupMode.InstantPopup)
        title_row.addWidget(more)
        root.addLayout(title_row)

        progress = QtWidgets.QProgressBar()
        progress.setRange(0, 1000)
        progress.setValue(round(torrent.progress * 1000))
        progress.setTextVisible(False)
        root.addWidget(progress)

        detail_row = QtWidgets.QHBoxLayout()
        detail_row.setSpacing(16)
        progress_text = QtWidgets.QLabel(
            f"{torrent.progress * 100:.1f}% · {format_bytes(torrent.completed)} из {format_bytes(torrent.total_size)}"
        )
        progress_text.setObjectName("Secondary")
        detail_row.addWidget(progress_text, 1)
        rates = QtWidgets.QLabel(
            f"↓ {format_rate(torrent.rate_download)}   ↑ {format_rate(torrent.rate_upload)}"
        )
        rates.setObjectName("Secondary")
        detail_row.addWidget(rates)
        peers = QtWidgets.QLabel(
            f"Пиры: {torrent.peers_sending_to_us}↓ {torrent.peers_getting_from_us}↑"
        )
        peers.setObjectName("Secondary")
        detail_row.addWidget(peers)
        root.addLayout(detail_row)


class SettingsDialog(QtWidgets.QDialog):
    def __init__(self, settings: AppSettings, parent: QtWidgets.QWidget) -> None:
        super().__init__(parent)
        self.setWindowTitle("Настройки")
        self.setMinimumWidth(520)
        layout = QtWidgets.QVBoxLayout(self)
        form = QtWidgets.QFormLayout()
        form.setSpacing(12)
        self.rpc = QtWidgets.QLineEdit(settings.rpc_url)
        self.rpc.setPlaceholderText("http://127.0.0.1:9191/mytorrents")
        form.addRow("Адрес RPC", self.rpc)

        dir_row = QtWidgets.QHBoxLayout()
        self.directory = QtWidgets.QLineEdit(settings.torrents_dir)
        browse = QtWidgets.QPushButton("Обзор…")
        browse.clicked.connect(self._browse)
        dir_row.addWidget(self.directory, 1)
        dir_row.addWidget(browse)
        form.addRow("Каталог торрентов", dir_row)
        self.interval = QtWidgets.QSpinBox()
        self.interval.setRange(2, 60)
        self.interval.setSuffix(" с")
        self.interval.setValue(settings.refresh_seconds)
        form.addRow("Обновление", self.interval)
        layout.addLayout(form)

        note = QtWidgets.QLabel(
            "GUI автоматически добавит /rpc/ к адресу. RPC требует i2pd, собранный с Boost 1.81 или новее."
        )
        note.setWordWrap(True)
        note.setObjectName("Secondary")
        layout.addWidget(note)
        buttons = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
            | QtWidgets.QDialogButtonBox.StandardButton.Save
        )
        buttons.accepted.connect(self._validate)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _browse(self) -> None:
        value = QtWidgets.QFileDialog.getExistingDirectory(self, "Каталог торрентов", self.directory.text())
        if value:
            self.directory.setText(value)

    def _validate(self) -> None:
        try:
            normalize_rpc_url(self.rpc.text())
        except ValueError as exc:
            QtWidgets.QMessageBox.warning(self, "Некорректный адрес", str(exc))
            return
        if not self.directory.text().strip():
            QtWidgets.QMessageBox.warning(self, "Некорректный каталог", "Укажите каталог торрентов")
            return
        self.accept()


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.settings = AppSettings.load()
        self.torrents: list[Torrent] = []
        self.filter_name = "all"
        self.busy = False
        self.pool = QtCore.QThreadPool.globalInstance()
        self._workers: set[Worker] = set()
        self.setWindowTitle("I2P Torrents")
        self.resize(1080, 700)
        self.setMinimumSize(780, 520)
        self._build_ui()
        self.apply_theme()
        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.refresh)
        self.timer.start(self.settings.refresh_seconds * 1000)
        QtCore.QTimer.singleShot(100, self.refresh)

    def _build_ui(self) -> None:
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        outer = QtWidgets.QHBoxLayout(central)
        outer.setContentsMargins(14, 14, 14, 14)
        outer.setSpacing(12)

        sidebar = QtWidgets.QFrame()
        sidebar.setObjectName("Sidebar")
        sidebar.setFixedWidth(210)
        side = QtWidgets.QVBoxLayout(sidebar)
        side.setContentsMargins(14, 16, 14, 14)
        title = QtWidgets.QLabel("I2P Torrents")
        title.setObjectName("Title")
        side.addWidget(title)
        subtitle = QtWidgets.QLabel("для i2pd")
        subtitle.setObjectName("Secondary")
        side.addWidget(subtitle)
        side.addSpacing(18)
        section = QtWidgets.QLabel("ТОРРЕНТЫ")
        section.setObjectName("SectionTitle")
        side.addWidget(section)
        self.filters: dict[str, QtWidgets.QPushButton] = {}
        group = QtWidgets.QButtonGroup(self)
        for key, label in (
            ("all", "Все"),
            ("downloading", "Загружаются"),
            ("seeding", "Раздаются"),
        ):
            button = QtWidgets.QPushButton(label)
            button.setObjectName("Filter")
            button.setCheckable(True)
            button.setChecked(key == "all")
            button.clicked.connect(lambda checked, k=key: self._set_filter(k))
            group.addButton(button)
            side.addWidget(button)
            self.filters[key] = button
        side.addStretch(1)
        self.theme_button = QtWidgets.QPushButton("Ночная тема" if self.settings.theme == "light" else "Светлая тема")
        self.theme_button.clicked.connect(self.toggle_theme)
        side.addWidget(self.theme_button)
        settings_button = QtWidgets.QPushButton("Настройки")
        settings_button.clicked.connect(self.open_settings)
        side.addWidget(settings_button)
        outer.addWidget(sidebar)

        surface = QtWidgets.QFrame()
        surface.setObjectName("Surface")
        body = QtWidgets.QVBoxLayout(surface)
        body.setContentsMargins(18, 16, 18, 16)
        body.setSpacing(12)
        top = QtWidgets.QHBoxLayout()
        heading = QtWidgets.QLabel("Торренты")
        heading.setObjectName("Title")
        top.addWidget(heading)
        top.addStretch(1)
        self.status = QtWidgets.QLabel("Подключение…")
        self.status.setObjectName("StatusOffline")
        top.addWidget(self.status)
        refresh = QtWidgets.QToolButton()
        refresh.setText("↻")
        refresh.setToolTip("Обновить")
        refresh.clicked.connect(self.refresh)
        top.addWidget(refresh)
        add = QtWidgets.QPushButton("Добавить торрент")
        add.setObjectName("Primary")
        add.clicked.connect(self.add_torrent)
        top.addWidget(add)
        body.addLayout(top)

        self.search = QtWidgets.QLineEdit()
        self.search.setPlaceholderText("Поиск по имени или info hash")
        self.search.setClearButtonEnabled(True)
        self.search.textChanged.connect(self.render)
        body.addWidget(self.search)
        self.summary = QtWidgets.QLabel("Нет данных")
        self.summary.setObjectName("Secondary")
        body.addWidget(self.summary)
        self.scroll = QtWidgets.QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.cards = QtWidgets.QWidget()
        self.cards_layout = QtWidgets.QVBoxLayout(self.cards)
        self.cards_layout.setContentsMargins(0, 0, 0, 0)
        self.cards_layout.setSpacing(10)
        self.cards_layout.addStretch(1)
        self.scroll.setWidget(self.cards)
        body.addWidget(self.scroll, 1)
        outer.addWidget(surface, 1)

    def rpc(self) -> TransmissionRPC:
        return TransmissionRPC(self.settings.rpc_url)

    def _run(self, call: Callable[[], Any], success: Callable[[Any], None]) -> None:
        worker = Worker(call)
        self._workers.add(worker)

        def done(value: Any) -> None:
            self._workers.discard(worker)
            success(value)

        def failed(message: str) -> None:
            self._workers.discard(worker)
            self._set_offline(message)

        worker.signals.succeeded.connect(done)
        worker.signals.failed.connect(failed)
        self.pool.start(worker)

    def refresh(self) -> None:
        if self.busy:
            return
        self.busy = True
        self.status.setText("Обновление…")
        self._run(self.rpc().get_torrents, self._on_torrents)

    def _on_torrents(self, torrents: object) -> None:
        self.busy = False
        self.torrents = list(torrents) if isinstance(torrents, list) else []
        self.status.setObjectName("StatusOnline")
        self.status.setText("i2pd подключён")
        self._repolish(self.status)
        self.render()

    def _set_offline(self, message: str) -> None:
        self.busy = False
        self.status.setObjectName("StatusOffline")
        self.status.setText("RPC недоступен")
        self.status.setToolTip(message)
        self._repolish(self.status)
        self.summary.setText(message)

    def _set_filter(self, name: str) -> None:
        self.filter_name = name
        self.render()

    def visible_torrents(self) -> list[Torrent]:
        query = self.search.text().strip().lower()
        rows = self.torrents
        if self.filter_name == "downloading":
            rows = [t for t in rows if t.status == TorrentStatus.DOWNLOADING]
        elif self.filter_name == "seeding":
            rows = [t for t in rows if t.status == TorrentStatus.SEEDING]
        if query:
            rows = [t for t in rows if query in t.name.lower() or query in t.hash_string.lower()]
        return sorted(rows, key=lambda t: (t.status != TorrentStatus.DOWNLOADING, t.name.lower()))

    def render(self) -> None:
        while self.cards_layout.count() > 1:
            item = self.cards_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()
        rows = self.visible_torrents()
        if rows:
            for torrent in rows:
                card = TorrentCard(torrent)
                card.removeRequested.connect(self.remove_torrent)
                self.cards_layout.insertWidget(self.cards_layout.count() - 1, card)
        else:
            empty = QtWidgets.QLabel(
                "Торренты не найдены.\nДобавьте .torrent-файл, чтобы начать загрузку."
            )
            empty.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
            empty.setObjectName("Secondary")
            self.cards_layout.insertWidget(0, empty)
        down = sum(t.rate_download for t in self.torrents)
        up = sum(t.rate_upload for t in self.torrents)
        self.summary.setText(f"{len(self.torrents)} торрентов  ·  ↓ {format_rate(down)}  ·  ↑ {format_rate(up)}")

    def add_torrent(self) -> None:
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Добавить торрент", "", "Torrent-файлы (*.torrent);;Все файлы (*)"
        )
        if not filename:
            return
        path = Path(filename)
        if self.status.objectName() == "StatusOnline":
            self._run(lambda: self.rpc().add_torrent(path), lambda _: self.refresh())
            return
        answer = QtWidgets.QMessageBox.question(
            self,
            "RPC недоступен",
            "Скопировать файл в torrentsdir?\n\n"
            "Без RPC запущенный i2pd может увидеть новый торрент только после перезапуска.",
        )
        if answer != QtWidgets.QMessageBox.StandardButton.Yes:
            return
        try:
            target_dir = Path(self.settings.torrents_dir).expanduser()
            target_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target_dir / path.name)
            QtWidgets.QMessageBox.information(self, "Файл добавлен", "Torrent-файл скопирован в torrentsdir.")
        except OSError as exc:
            QtWidgets.QMessageBox.critical(self, "Ошибка", f"Не удалось скопировать файл:\n{exc}")

    def remove_torrent(self, torrent_id: int, name: str) -> None:
        box = QtWidgets.QMessageBox(self)
        box.setWindowTitle("Удалить торрент")
        box.setText(f"Удалить «{name}» из i2pd?")
        delete = QtWidgets.QCheckBox("Также удалить загруженные данные")
        box.setCheckBox(delete)
        box.setStandardButtons(
            QtWidgets.QMessageBox.StandardButton.Cancel | QtWidgets.QMessageBox.StandardButton.Yes
        )
        box.setDefaultButton(QtWidgets.QMessageBox.StandardButton.Cancel)
        if box.exec() == QtWidgets.QMessageBox.StandardButton.Yes:
            self._run(
                lambda: self.rpc().remove_torrent(torrent_id, delete.isChecked()),
                lambda _: self.refresh(),
            )

    def open_settings(self) -> None:
        dialog = SettingsDialog(self.settings, self)
        dialog.setStyleSheet(stylesheet(self.settings.theme))
        if dialog.exec() != QtWidgets.QDialog.DialogCode.Accepted:
            return
        self.settings.rpc_url = dialog.rpc.text().strip()
        self.settings.torrents_dir = dialog.directory.text().strip()
        self.settings.refresh_seconds = dialog.interval.value()
        self.settings.save()
        self.timer.setInterval(self.settings.refresh_seconds * 1000)
        self.refresh()

    def toggle_theme(self) -> None:
        self.settings.theme = "night" if self.settings.theme == "light" else "light"
        self.settings.save()
        self.theme_button.setText("Ночная тема" if self.settings.theme == "light" else "Светлая тема")
        self.apply_theme()

    def apply_theme(self) -> None:
        self.setStyleSheet(stylesheet(self.settings.theme))

    @staticmethod
    def _repolish(widget: QtWidgets.QWidget) -> None:
        widget.style().unpolish(widget)
        widget.style().polish(widget)


def main() -> int:
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("I2P Torrents")
    app.setOrganizationName("MetanoicArmor")
    app.setStyle("Fusion")
    font = app.font()
    font.setFamilies(["Inter", "SF Pro Text", "Segoe UI", "Noto Sans", "sans-serif"])
    app.setFont(font)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
