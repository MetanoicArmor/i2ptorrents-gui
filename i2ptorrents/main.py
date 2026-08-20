from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Callable

from PyQt6 import QtCore, QtGui, QtWidgets

from . import APP_AUTHOR, APP_LICENSE, APP_NAME, __version__, application_icon_path
from .i18n import language, normalize_language, set_language, t
from .magnet import MagnetError, download_torrent_from_magnet, normalize_http_proxy, parse_magnet
from .models import Torrent, TorrentStatus, format_bytes, format_rate
from .rounded_tooltip import apply_tooltip_palette, install_rounded_tooltips, install_tooltip_wakeup_delay_style
from .rpc import TransmissionRPC, normalize_rpc_url
from .settings import AppSettings
from .styled_popups import ActionsPopup, OverlayScrollArea, StyledCombo, wrap_spin_row
from .theme import stylesheet


def _native_shortcut(*portable: str) -> str:
    seen: list[str] = []
    for sequence in portable:
        text = QtGui.QKeySequence(sequence).toString(QtGui.QKeySequence.SequenceFormat.NativeText)
        if text and text not in seen:
            seen.append(text)
    return ", ".join(seen)


def _tip_with_shortcut(label: str, *portable: str) -> str:
    native = _native_shortcut(*portable)
    return f"{label} ({native})" if native else label


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


class PieceMap(QtWidgets.QWidget):
    def __init__(self, have: tuple[bool, ...], parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self._have = have
        self.setObjectName("PieceMap")
        self.setFixedHeight(10)
        self.setSizePolicy(
            QtWidgets.QSizePolicy.Policy.Expanding,
            QtWidgets.QSizePolicy.Policy.Fixed,
        )
        have_n = sum(have)
        self.setToolTip(t("pieces_tooltip", have=have_n, total=len(have)))

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: ARG002
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, False)
        rect = self.rect().adjusted(0, 0, -1, -1)
        dark = self.palette().color(QtGui.QPalette.ColorRole.WindowText).lightness() > 140
        background = QtGui.QColor("#3b3e48" if dark else "#d6d7dd")
        have_color = QtGui.QColor("#30d158" if dark else "#248a3d")
        partial = QtGui.QColor("#8de5a8" if dark else "#6bcf80")
        painter.setPen(QtCore.Qt.PenStyle.NoPen)
        painter.setBrush(background)
        painter.drawRoundedRect(rect, 3, 3)
        if not self._have or rect.width() <= 0:
            return
        total = len(self._have)
        width = max(1, rect.width())
        for column in range(width):
            start = column * total // width
            end = max(start + 1, (column + 1) * total // width)
            chunk = self._have[start:end]
            if all(chunk):
                painter.fillRect(rect.x() + column, rect.y(), 1, rect.height(), have_color)
            elif any(chunk):
                painter.fillRect(rect.x() + column, rect.y(), 1, rect.height(), partial)


class TorrentCard(QtWidgets.QFrame):
    removeRequested = QtCore.pyqtSignal(int, str)
    menuClosed = QtCore.pyqtSignal()

    def __init__(
        self,
        torrent: Torrent,
        downloads_dir: Path,
        detailed: bool = True,
        theme: str = "light",
        parent: QtWidgets.QWidget | None = None,
    ) -> None:
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
        more.setToolTip(t("actions"))
        more.clicked.connect(self._show_actions)
        title_row.addWidget(more)
        root.addLayout(title_row)
        self._more = more
        self._theme = theme
        self._hash = torrent.hash_string
        self._magnet = torrent.magnet_uri()
        self._downloads_dir = downloads_dir
        self._torrent_name = torrent.name
        self._torrent_id = torrent.id
        self._actions: ActionsPopup | None = None

        progress = QtWidgets.QProgressBar()
        progress.setRange(0, 1000)
        progress.setValue(round(torrent.progress * 1000))
        progress.setTextVisible(False)
        progress.setToolTip(t("download_progress"))
        root.addWidget(progress)
        if detailed and torrent.pieces:
            root.addWidget(PieceMap(torrent.pieces))

        detail_row = QtWidgets.QHBoxLayout()
        detail_row.setSpacing(16)
        progress_text = QtWidgets.QLabel(
            t(
                "progress_of",
                percent=torrent.progress * 100,
                done=format_bytes(torrent.completed),
                total=format_bytes(torrent.total_size),
            )
        )
        progress_text.setObjectName("Secondary")
        detail_row.addWidget(progress_text, 1)
        rates = QtWidgets.QLabel(
            f"↓ {format_rate(torrent.rate_download)}   ↑ {format_rate(torrent.rate_upload)}"
        )
        rates.setObjectName("Secondary")
        detail_row.addWidget(rates)
        peers = QtWidgets.QLabel(
            t("peers", down=torrent.peers_sending_to_us, up=torrent.peers_getting_from_us)
        )
        peers.setObjectName("Secondary")
        detail_row.addWidget(peers)
        root.addLayout(detail_row)

        if detailed:
            meta = []
            if torrent.hash_string:
                meta.append(torrent.short_hash)
            if torrent.piece_count:
                meta.append(t("pieces_meta", count=torrent.piece_count, size=format_bytes(torrent.piece_size)))
            if meta:
                info = QtWidgets.QLabel("  ·  ".join(meta))
                info.setObjectName("Secondary")
                info.setTextInteractionFlags(QtCore.Qt.TextInteractionFlag.TextSelectableByMouse)
                if torrent.hash_string:
                    info.setToolTip(torrent.hash_string.lower())
                root.addWidget(info)

    def _copy(self, text: str) -> None:
        if not text:
            return
        QtWidgets.QApplication.clipboard().setText(text)

    def _open_folder(self, root: Path, name: str) -> None:
        target = root / name
        if not target.exists():
            part = Path(str(target) + ".part")
            target = part if part.exists() else root
        if target.is_file():
            target = target.parent
        QtGui.QDesktopServices.openUrl(QtCore.QUrl.fromLocalFile(str(target)))

    def menu_open(self) -> bool:
        popup = self._actions
        if popup is None:
            return False
        try:
            return popup.isVisible()
        except RuntimeError:
            return False

    def _show_actions(self) -> None:
        if self.menu_open():
            self._actions.hide()
            return
        popup = ActionsPopup(self)
        popup.apply_theme(self._theme)
        popup.add_action(
            t("copy_hash"),
            lambda: self._copy(self._hash.lower()),
            enabled=bool(self._hash),
        )
        popup.add_action(
            t("copy_magnet"),
            lambda: self._copy(self._magnet),
            enabled=bool(self._magnet),
        )
        popup.add_action(t("open_folder"), lambda: self._open_folder(self._downloads_dir, self._torrent_name))
        popup.add_separator()
        popup.add_action(
            t("remove_from_list"),
            lambda: self.removeRequested.emit(self._torrent_id, self._torrent_name),
        )
        popup.closed.connect(self.menuClosed.emit)
        self._actions = popup
        popup.show_below(self._more)


class AboutDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget) -> None:
        super().__init__(parent)
        self.setMinimumWidth(420)
        layout = QtWidgets.QVBoxLayout(self)
        self.heading = QtWidgets.QLabel(APP_NAME)
        self.heading.setObjectName("Title")
        layout.addWidget(self.heading)
        self.body = QtWidgets.QLabel()
        self.body.setWordWrap(True)
        self.body.setObjectName("Secondary")
        self.body.setTextInteractionFlags(
            QtCore.Qt.TextInteractionFlag.TextSelectableByMouse
        )
        layout.addWidget(self.body)
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.StandardButton.Ok)
        buttons.accepted.connect(self.accept)
        self.ok_button = buttons.button(QtWidgets.QDialogButtonBox.StandardButton.Ok)
        layout.addWidget(buttons)
        self.retranslate()

    def retranslate(self) -> None:
        self.setWindowTitle(t("about_title"))
        self.body.setText(
            t("about_body", version=__version__, author=APP_AUTHOR, license=APP_LICENSE)
        )
        self.ok_button.setText(t("ok"))


class SettingsDialog(QtWidgets.QDialog):
    def __init__(self, settings: AppSettings, parent: QtWidgets.QWidget) -> None:
        super().__init__(parent)
        self.setMinimumWidth(520)
        layout = QtWidgets.QVBoxLayout(self)
        form = QtWidgets.QFormLayout()
        form.setSpacing(12)
        self.rpc = QtWidgets.QLineEdit(settings.rpc_url)
        self.rpc.setPlaceholderText("http://127.0.0.1:9191/mytorrents")
        self.rpc_label = QtWidgets.QLabel()
        form.addRow(self.rpc_label, self.rpc)

        dir_row = QtWidgets.QHBoxLayout()
        self.directory = QtWidgets.QLineEdit(settings.torrents_dir)
        self.browse = QtWidgets.QPushButton()
        self.browse.clicked.connect(self._browse)
        dir_row.addWidget(self.directory, 1)
        dir_row.addWidget(self.browse)
        self.directory_label = QtWidgets.QLabel()
        form.addRow(self.directory_label, dir_row)
        self.interval = QtWidgets.QSpinBox()
        self.interval.setRange(2, 60)
        self.interval.setValue(settings.refresh_seconds)
        self.interval_label = QtWidgets.QLabel()
        form.addRow(self.interval_label, wrap_spin_row(self.interval))
        self.proxy = QtWidgets.QLineEdit(settings.http_proxy)
        self.proxy.setPlaceholderText("socks5://127.0.0.1:4447")
        self.proxy_label = QtWidgets.QLabel()
        form.addRow(self.proxy_label, self.proxy)
        self.language = StyledCombo()
        self.language.addItem(t("language_name_en"), "en")
        self.language.addItem(t("language_name_ru"), "ru")
        index = self.language.findData(normalize_language(settings.language))
        self.language.setCurrentIndex(max(0, index))
        self.language.currentIndexChanged.connect(self._preview_language)
        self.language_label = QtWidgets.QLabel()
        form.addRow(self.language_label, self.language)
        self.theme = StyledCombo()
        self.theme_label = QtWidgets.QLabel()
        form.addRow(self.theme_label, self.theme)
        self.torrent_view = StyledCombo()
        self.torrent_view_label = QtWidgets.QLabel()
        form.addRow(self.torrent_view_label, self.torrent_view)
        layout.addLayout(form)

        self.note = QtWidgets.QLabel()
        self.note.setWordWrap(True)
        self.note.setObjectName("Secondary")
        layout.addWidget(self.note)
        self.buttons = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
            | QtWidgets.QDialogButtonBox.StandardButton.Save
        )
        self.buttons.accepted.connect(self._validate)
        self.buttons.rejected.connect(self.reject)
        layout.addWidget(self.buttons)
        self._fill_theme(settings.theme)
        self._fill_view(settings.torrent_view)
        self._apply_combo_theme(settings.theme)
        self.theme.currentIndexChanged.connect(self._preview_combo_theme)
        self.retranslate()

    def _fill_theme(self, selected: str) -> None:
        current = selected or self.theme.currentData() or "light"
        self.theme.blockSignals(True)
        self.theme.clear()
        self.theme.addItem(t("theme_light"), "light")
        self.theme.addItem(t("theme_night"), "night")
        index = self.theme.findData("night" if current == "night" else "light")
        self.theme.setCurrentIndex(max(0, index))
        self.theme.blockSignals(False)

    def _fill_view(self, selected: str) -> None:
        current = selected or self.torrent_view.currentData() or "detailed"
        self.torrent_view.blockSignals(True)
        self.torrent_view.clear()
        self.torrent_view.addItem(t("view_simple"), "simple")
        self.torrent_view.addItem(t("view_detailed"), "detailed")
        index = self.torrent_view.findData("simple" if current == "simple" else "detailed")
        self.torrent_view.setCurrentIndex(max(0, index))
        self.torrent_view.blockSignals(False)

    def _apply_combo_theme(self, theme: str) -> None:
        for combo in (self.language, self.theme, self.torrent_view):
            combo.apply_theme(theme)

    def _preview_combo_theme(self) -> None:
        self._apply_combo_theme("night" if self.theme.currentData() == "night" else "light")

    def _preview_language(self) -> None:
        set_language(str(self.language.currentData() or "en"))
        self._fill_theme(str(self.theme.currentData() or "light"))
        self._fill_view(str(self.torrent_view.currentData() or "detailed"))
        self.retranslate()

    def retranslate(self) -> None:
        self.setWindowTitle(t("settings_title"))
        self.rpc_label.setText(t("rpc_address"))
        self.rpc.setToolTip(t("rpc_setup_tip"))
        self.browse.setText(t("browse"))
        self.directory_label.setText(t("torrents_directory"))
        self.interval.setSuffix(t("seconds_suffix"))
        self.interval_label.setText(t("refresh_interval"))
        self.proxy.setToolTip(t("proxy_tip"))
        self.proxy_label.setText(t("i2p_proxy"))
        self.language_label.setText(t("language"))
        self.theme_label.setText(t("theme"))
        self.torrent_view_label.setText(t("torrent_view"))
        self.note.setText(t("settings_note"))
        self.note.setToolTip(t("rpc_setup_tip"))
        self.buttons.button(QtWidgets.QDialogButtonBox.StandardButton.Save).setText(t("save"))
        self.buttons.button(QtWidgets.QDialogButtonBox.StandardButton.Cancel).setText(t("cancel"))

    def _browse(self) -> None:
        value = QtWidgets.QFileDialog.getExistingDirectory(
            self, t("torrents_directory"), self.directory.text()
        )
        if value:
            self.directory.setText(value)

    def _validate(self) -> None:
        try:
            normalize_rpc_url(self.rpc.text())
            normalize_http_proxy(self.proxy.text())
        except ValueError as exc:
            QtWidgets.QMessageBox.warning(self, t("invalid_address"), str(exc))
            return
        if not self.directory.text().strip():
            QtWidgets.QMessageBox.warning(self, t("invalid_directory"), t("need_torrents_dir"))
            return
        self.accept()


class AddTorrentDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget) -> None:
        super().__init__(parent)
        self.setMinimumWidth(560)
        layout = QtWidgets.QVBoxLayout(self)
        self.label = QtWidgets.QLabel()
        layout.addWidget(self.label)
        row = QtWidgets.QHBoxLayout()
        self.source = QtWidgets.QLineEdit()
        clip = QtWidgets.QApplication.clipboard().text().strip()
        if clip.lower().startswith("magnet:"):
            self.source.setText(clip)
        self.browse = QtWidgets.QPushButton()
        self.browse.clicked.connect(self._browse)
        row.addWidget(self.source, 1)
        row.addWidget(self.browse)
        layout.addLayout(row)
        self.buttons = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
            | QtWidgets.QDialogButtonBox.StandardButton.Ok
        )
        self.buttons.accepted.connect(self._validate)
        self.buttons.rejected.connect(self.reject)
        layout.addWidget(self.buttons)
        self.retranslate()

    def retranslate(self) -> None:
        self.setWindowTitle(t("add_title"))
        self.label.setText(t("add_label"))
        self.source.setPlaceholderText(t("add_placeholder"))
        self.browse.setText(t("file_button"))
        self.buttons.button(QtWidgets.QDialogButtonBox.StandardButton.Ok).setText(t("add_ok"))
        self.buttons.button(QtWidgets.QDialogButtonBox.StandardButton.Cancel).setText(t("cancel"))

    def _browse(self) -> None:
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self,
            t("add_title"),
            "",
            f"{t('torrent_files')} (*.torrent);;{t('all_files')} (*)",
        )
        if filename:
            self.source.setText(filename)

    def _validate(self) -> None:
        text = self.source.text().strip()
        if not text:
            QtWidgets.QMessageBox.warning(self, t("empty_source"), t("empty_source_text"))
            return
        if text.lower().startswith("magnet:"):
            try:
                parse_magnet(text)
            except MagnetError as exc:
                QtWidgets.QMessageBox.warning(self, t("bad_magnet"), str(exc))
                return
        elif not Path(text).expanduser().is_file():
            QtWidgets.QMessageBox.warning(self, t("file_not_found"), t("file_not_found_text"))
            return
        self.accept()


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.settings = AppSettings.load()
        set_language(self.settings.language)
        self.torrents: list[Torrent] = []
        self.filter_name = "all"
        self.busy = False
        self._adding = False
        self._status_mode = "connecting"
        self._status_detail = ""
        self.pool = QtCore.QThreadPool.globalInstance()
        self._workers: set[Worker] = set()
        self.setWindowTitle(f"{APP_NAME} {__version__}")
        self.resize(1080, 700)
        self.setMinimumSize(780, 520)
        self._build_ui()
        self._install_shortcuts()
        self.apply_theme()
        try:
            self.settings.torrents_path()
        except OSError:
            pass
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
        self.subtitle = QtWidgets.QLabel()
        self.subtitle.setObjectName("Secondary")
        side.addWidget(self.subtitle)
        side.addSpacing(18)
        self.section = QtWidgets.QLabel()
        self.section.setObjectName("SectionTitle")
        side.addWidget(self.section)
        self.filters: dict[str, QtWidgets.QPushButton] = {}
        group = QtWidgets.QButtonGroup(self)
        for key in ("all", "downloading", "seeding"):
            button = QtWidgets.QPushButton()
            button.setObjectName("Filter")
            button.setCheckable(True)
            button.setChecked(key == "all")
            button.clicked.connect(lambda checked, k=key: self._set_filter(k))
            group.addButton(button)
            side.addWidget(button)
            self.filters[key] = button
        side.addStretch(1)
        self.about_button = QtWidgets.QPushButton()
        self.about_button.clicked.connect(self.open_about)
        side.addWidget(self.about_button)
        self.settings_button = QtWidgets.QPushButton()
        self.settings_button.clicked.connect(self.open_settings)
        side.addWidget(self.settings_button)
        outer.addWidget(sidebar)

        surface = QtWidgets.QFrame()
        surface.setObjectName("Surface")
        body = QtWidgets.QVBoxLayout(surface)
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(0)

        head = QtWidgets.QWidget()
        head_layout = QtWidgets.QVBoxLayout(head)
        head_layout.setContentsMargins(18, 16, 18, 0)
        head_layout.setSpacing(12)
        top = QtWidgets.QHBoxLayout()
        self.heading = QtWidgets.QLabel()
        self.heading.setObjectName("Title")
        top.addWidget(self.heading)
        top.addStretch(1)
        self.status = QtWidgets.QLabel()
        self.status.setObjectName("StatusOffline")
        self.status.setCursor(QtCore.Qt.CursorShape.WhatsThisCursor)
        top.addWidget(self.status)
        self.refresh_button = QtWidgets.QToolButton()
        self.refresh_button.setText("↻")
        self.refresh_button.clicked.connect(self.refresh)
        top.addWidget(self.refresh_button)
        self.add_button = QtWidgets.QPushButton()
        self.add_button.setObjectName("Primary")
        self.add_button.clicked.connect(self.add_torrent)
        top.addWidget(self.add_button)
        head_layout.addLayout(top)

        self.search = QtWidgets.QLineEdit()
        self.search.setClearButtonEnabled(True)
        self.search.textChanged.connect(self.render)
        head_layout.addWidget(self.search)
        self.summary = QtWidgets.QLabel()
        self.summary.setObjectName("Secondary")
        head_layout.addWidget(self.summary)
        body.addWidget(head)

        self.scroll = OverlayScrollArea()
        self.cards = QtWidgets.QWidget()
        self.cards_layout = QtWidgets.QVBoxLayout(self.cards)
        self.cards_layout.setContentsMargins(18, 12, 14, 16)
        self.cards_layout.setSpacing(10)
        self.cards_layout.addStretch(1)
        self.scroll.setWidget(self.cards)
        body.addWidget(self.scroll, 1)
        outer.addWidget(surface, 1)
        self.retranslate()

    def _install_shortcuts(self) -> None:
        for sequence, slot in (
            ("Ctrl+T", self.add_torrent),
            ("Ctrl+O", self.open_torrent_file),
            ("Ctrl+S", self.open_settings),
            ("Ctrl+,", self.open_settings),
        ):
            shortcut = QtGui.QShortcut(QtGui.QKeySequence(sequence), self)
            shortcut.setContext(QtCore.Qt.ShortcutContext.WindowShortcut)
            shortcut.activated.connect(slot)

    def retranslate(self) -> None:
        self.setWindowTitle(f"{APP_NAME} {__version__}")
        self.subtitle.setText(t("subtitle", version=__version__))
        self.section.setText(t("section_torrents"))
        self.filters["all"].setText(t("filter_all"))
        self.filters["downloading"].setText(t("filter_downloading"))
        self.filters["seeding"].setText(t("filter_seeding"))
        self.about_button.setText(t("about"))
        self.settings_button.setText(t("settings"))
        self.settings_button.setToolTip(_tip_with_shortcut(t("settings"), "Ctrl+S", "Ctrl+,"))
        self.heading.setText(t("heading_torrents"))
        self.refresh_button.setToolTip(t("refresh"))
        self.add_button.setText(t("add_torrent"))
        self.add_button.setToolTip(_tip_with_shortcut(t("add_torrent"), "Ctrl+T"))
        self.search.setPlaceholderText(t("search_placeholder"))
        if self.status.objectName() == "StatusOnline":
            self._status_mode = "online"
        self._refresh_status_text()
        self.render()

    def _refresh_status_text(self) -> None:
        mode = self._status_mode
        if mode == "online":
            self.status.setText(t("rpc_online"))
            self.status.setToolTip("")
            self.status.setCursor(QtCore.Qt.CursorShape.ArrowCursor)
        elif mode == "updating":
            self.status.setText(t("updating"))
            self.status.setCursor(QtCore.Qt.CursorShape.ArrowCursor)
        elif mode == "magnet":
            self.status.setText(t("magnet_wait"))
            self.status.setToolTip(t("magnet_wait_tip"))
            self.status.setCursor(QtCore.Qt.CursorShape.ArrowCursor)
        elif mode == "copying":
            self.status.setText(t("copying_torrent"))
            self.status.setCursor(QtCore.Qt.CursorShape.ArrowCursor)
        elif mode == "offline":
            self.status.setText(t("rpc_offline"))
            tip = t("rpc_setup_tip")
            if self._status_detail:
                tip = f"{self._status_detail}\n\n{tip}"
            self.status.setToolTip(tip)
            self.status.setCursor(QtCore.Qt.CursorShape.WhatsThisCursor)
        else:
            self.status.setText(t("connecting"))
            self.status.setToolTip(t("rpc_setup_tip"))
            self.status.setCursor(QtCore.Qt.CursorShape.WhatsThisCursor)

    def rpc(self) -> TransmissionRPC:
        return TransmissionRPC(self.settings.rpc_url)

    def _run(
        self,
        call: Callable[[], Any],
        success: Callable[[Any], None],
        error: Callable[[str], None] | None = None,
    ) -> None:
        worker = Worker(call)
        self._workers.add(worker)

        def done(value: Any) -> None:
            self._workers.discard(worker)
            success(value)

        def failed(message: str) -> None:
            self._workers.discard(worker)
            if error is not None:
                error(message)
            else:
                self._set_offline(message)

        worker.signals.succeeded.connect(done)
        worker.signals.failed.connect(failed)
        self.pool.start(worker)

    def refresh(self) -> None:
        if self.busy or self._adding:
            return
        self.busy = True
        self._status_mode = "updating"
        self._refresh_status_text()
        self._run(lambda: self.rpc().get_torrents(detailed=self.settings.torrent_view != "simple"), self._on_torrents)

    def _on_torrents(self, torrents: object) -> None:
        self.busy = False
        self.torrents = list(torrents) if isinstance(torrents, list) else []
        self.status.setObjectName("StatusOnline")
        self._status_mode = "online"
        self._status_detail = ""
        self._refresh_status_text()
        self._repolish(self.status)
        self.render()

    def _set_offline(self, message: str) -> None:
        self.busy = False
        self.status.setObjectName("StatusOffline")
        self._status_mode = "offline"
        self._status_detail = message
        self._refresh_status_text()
        self._repolish(self.status)
        self.summary.setText(message)
        self.summary.setToolTip(f"{message}\n\n{t('rpc_setup_tip')}")

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

    def _card_menu_open(self) -> bool:
        for index in range(self.cards_layout.count()):
            widget = self.cards_layout.itemAt(index).widget()
            if isinstance(widget, TorrentCard) and widget.menu_open():
                return True
        return False

    def _render_after_menu(self) -> None:
        QtCore.QTimer.singleShot(0, self.render)

    def render(self) -> None:
        down = sum(t.rate_download for t in self.torrents)
        up = sum(t.rate_upload for t in self.torrents)
        self.summary.setText(
            t("summary", count=len(self.torrents), down=format_rate(down), up=format_rate(up))
        )
        if self.status.objectName() == "StatusOnline":
            self.summary.setToolTip("")
        if self._card_menu_open():
            return
        while self.cards_layout.count() > 1:
            item = self.cards_layout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()
        rows = self.visible_torrents()
        downloads = Path(self.settings.torrents_dir).expanduser()
        if rows:
            for torrent in rows:
                card = TorrentCard(
                    torrent,
                    downloads,
                    detailed=self.settings.torrent_view != "simple",
                    theme=self.settings.theme,
                )
                card.removeRequested.connect(self.remove_torrent)
                card.menuClosed.connect(self._render_after_menu)
                self.cards_layout.insertWidget(self.cards_layout.count() - 1, card)
        else:
            empty = QtWidgets.QLabel(t("empty_list", path=self.settings.torrents_dir))
            empty.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
            empty.setObjectName("Secondary")
            self.cards_layout.insertWidget(0, empty)
        down = sum(t.rate_download for t in self.torrents)
        up = sum(t.rate_upload for t in self.torrents)
        self.summary.setText(
            t("summary", count=len(self.torrents), down=format_rate(down), up=format_rate(up))
        )
        if self.status.objectName() == "StatusOnline":
            self.summary.setToolTip("")

    def add_torrent(self) -> None:
        if self._adding:
            return
        dialog = AddTorrentDialog(self)
        dialog.setStyleSheet(stylesheet(self.settings.theme))
        if dialog.exec() != QtWidgets.QDialog.DialogCode.Accepted:
            return
        self._start_add(dialog.source.text().strip())

    def open_torrent_file(self) -> None:
        if self._adding:
            return
        filename, _ = QtWidgets.QFileDialog.getOpenFileName(
            self,
            t("add_title"),
            "",
            f"{t('torrent_files')} (*.torrent);;{t('all_files')} (*)",
        )
        if filename:
            self._start_add(filename)

    def _start_add(self, source: str) -> None:
        source = source.strip()
        if not source:
            return
        rpc_online = self.status.objectName() == "StatusOnline"
        self._adding = True
        if source.lower().startswith("magnet:"):
            self._status_mode = "magnet"
            self._refresh_status_text()
            self._run(
                lambda: self._import_magnet(source, rpc_online),
                self._on_added,
                self._on_add_failed,
            )
            return
        path = Path(source).expanduser()
        self._status_mode = "copying"
        self._refresh_status_text()
        self._run(
            lambda: self._import_file(path, rpc_online),
            self._on_added,
            self._on_add_failed,
        )

    def _on_add_failed(self, message: str) -> None:
        self._adding = False
        QtWidgets.QMessageBox.warning(self, t("add_failed"), message)
        self.refresh()

    def _on_added(self, result: object) -> None:
        self._adding = False
        saved = result.get("saved") if isinstance(result, dict) else None
        if saved:
            QtWidgets.QMessageBox.information(self, t("added_title"), t("added_body", path=saved))
        self.refresh()

    def _import_magnet(self, source: str, rpc_online: bool) -> dict:
        content, magnet = download_torrent_from_magnet(source, self.settings.http_proxy)
        saved = self._write_torrent(content, magnet.filename)
        added: dict = {"saved": str(saved)}
        if rpc_online:
            added.update(self.rpc().add_torrent_bytes(content))
        return added

    def _import_file(self, path: Path, rpc_online: bool) -> dict:
        content = path.read_bytes()
        if not content:
            raise OSError(t("rpc_empty_file"))
        saved = self._write_torrent(content, path.name)
        added: dict = {"saved": str(saved)}
        if rpc_online:
            added.update(self.rpc().add_torrent_bytes(content))
        return added

    def _write_torrent(self, content: bytes, filename: str) -> Path:
        target_dir = self.settings.torrents_path()
        dest = target_dir / filename
        dest.write_bytes(content)
        return dest

    def remove_torrent(self, torrent_id: int, name: str) -> None:
        box = QtWidgets.QMessageBox(self)
        box.setWindowTitle(t("remove_title"))
        box.setText(t("remove_text", name=name))
        delete = QtWidgets.QCheckBox(t("remove_data"))
        box.setCheckBox(delete)
        box.setStandardButtons(
            QtWidgets.QMessageBox.StandardButton.Cancel | QtWidgets.QMessageBox.StandardButton.Yes
        )
        box.button(QtWidgets.QMessageBox.StandardButton.Yes).setText(t("yes"))
        box.button(QtWidgets.QMessageBox.StandardButton.Cancel).setText(t("cancel"))
        box.setDefaultButton(QtWidgets.QMessageBox.StandardButton.Cancel)
        if box.exec() == QtWidgets.QMessageBox.StandardButton.Yes:
            self._run(
                lambda: self.rpc().remove_torrent(torrent_id, delete.isChecked()),
                lambda _: self.refresh(),
            )

    def open_about(self) -> None:
        dialog = AboutDialog(self)
        dialog.setStyleSheet(stylesheet(self.settings.theme))
        dialog.exec()

    def open_settings(self) -> None:
        previous_language = language()
        dialog = SettingsDialog(self.settings, self)
        dialog.setStyleSheet(stylesheet(self.settings.theme))
        if dialog.exec() != QtWidgets.QDialog.DialogCode.Accepted:
            set_language(previous_language)
            return
        self.settings.rpc_url = dialog.rpc.text().strip()
        self.settings.torrents_dir = dialog.directory.text().strip()
        self.settings.refresh_seconds = dialog.interval.value()
        self.settings.http_proxy = normalize_http_proxy(dialog.proxy.text())
        self.settings.language = normalize_language(str(dialog.language.currentData()))
        self.settings.theme = "night" if dialog.theme.currentData() == "night" else "light"
        self.settings.torrent_view = "simple" if dialog.torrent_view.currentData() == "simple" else "detailed"
        set_language(self.settings.language)
        try:
            self.settings.torrents_path()
        except OSError as exc:
            QtWidgets.QMessageBox.warning(
                self, t("torrents_directory"), t("torrents_dir_error", error=exc)
            )
        self.settings.save()
        self.timer.setInterval(self.settings.refresh_seconds * 1000)
        self.apply_theme()
        self.retranslate()
        self.refresh()

    def apply_theme(self) -> None:
        sheet = stylesheet(self.settings.theme)
        self.setStyleSheet(sheet)
        app = QtWidgets.QApplication.instance()
        if isinstance(app, QtWidgets.QApplication):
            app.setStyleSheet(sheet)
            apply_tooltip_palette(app, self.settings.theme)
        self.scroll.apply_theme(self.settings.theme)

    @staticmethod
    def _repolish(widget: QtWidgets.QWidget) -> None:
        widget.style().unpolish(widget)
        widget.style().polish(widget)


def main() -> int:
    if sys.platform == "darwin":
        QtWidgets.QApplication.setAttribute(
            QtCore.Qt.ApplicationAttribute.AA_DontUseNativeMenuWindows, True
        )
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setApplicationVersion(__version__)
    app.setOrganizationName("MetanoicArmor")
    app.setStyle("Fusion")
    icon_path = application_icon_path()
    if icon_path is not None:
        app.setWindowIcon(QtGui.QIcon(str(icon_path)))
    install_tooltip_wakeup_delay_style(app)
    install_rounded_tooltips(app)
    font_pt = 10 if sys.platform == "win32" else 13
    if sys.platform == "darwin":
        font = app.font()
        font.setPointSize(font_pt)
    else:
        font = QtGui.QFont("Inter", font_pt)
        font.setFamilies(["Inter", "Segoe UI", "Noto Sans", "sans-serif"])
    font.setStyleHint(QtGui.QFont.StyleHint.SansSerif)
    app.setFont(font)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
