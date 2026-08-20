"""Кастомные выпадающие списки и меню действий (как в I2PChat)."""

from __future__ import annotations

import sys
from typing import Any, Callable, Optional

from PyQt6 import QtCore, QtGui, QtWidgets

from .popup_geometry import (
    disable_dwm_rounded_frame,
    global_position_popup_below_anchor,
    paint_popup_rounded_bg,
    popup_window_flags,
    update_popup_rounded_mask,
)


def _combo_arrow_color(theme: str) -> str:
    return "#9fa1b5" if theme == "night" else "#8c8d94"


class RoundedVerticalScrollbar(QtWidgets.QWidget):
    """Кастомный скроллбар с пилюльным ползунком, как в I2PChat."""

    def __init__(
        self,
        linked_scrollbar: QtWidgets.QScrollBar,
        parent: Optional[QtWidgets.QWidget] = None,
    ) -> None:
        super().__init__(parent)
        self._sb = linked_scrollbar
        self._thumb_color = QtGui.QColor(60, 60, 67, 89)
        self._track_color = QtGui.QColor(0, 0, 0, 0)
        self._dragging = False
        self._drag_offset_y = 0
        self.setFixedWidth(6)
        self.setCursor(QtCore.Qt.CursorShape.PointingHandCursor)
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Fixed, QtWidgets.QSizePolicy.Policy.Expanding)
        self._sb.valueChanged.connect(self.update)
        self._sb.rangeChanged.connect(self.update)

    def set_colors(self, thumb: QtGui.QColor, track: QtGui.QColor) -> None:
        self._thumb_color = thumb
        self._track_color = track
        self.update()

    def apply_theme(self, theme: str) -> None:
        if theme == "night":
            self.set_colors(QtGui.QColor(255, 255, 255, 51), QtGui.QColor(0, 0, 0, 0))
        else:
            self.set_colors(QtGui.QColor(60, 60, 67, 89), QtGui.QColor(0, 0, 0, 0))

    def _compute_thumb(self) -> QtCore.QRectF:
        track_h = max(0, self.height())
        if track_h <= 0:
            return QtCore.QRectF(0, 0, float(self.width()), 0)
        sb_min, sb_max = self._sb.minimum(), self._sb.maximum()
        sb_range = max(0, sb_max - sb_min)
        page = max(0, int(self._sb.pageStep()))
        total = sb_range + page
        visible_ratio = (page / total) if total else 1.0
        thumb_h = max(16.0, min(float(track_h), float(track_h) * max(0.05, min(1.0, visible_ratio))))
        progress = 0.0 if sb_range <= 0 else max(0.0, min(1.0, float(self._sb.value() - sb_min) / float(sb_range)))
        return QtCore.QRectF(0.0, (float(track_h) - thumb_h) * progress, float(self.width()), thumb_h)

    def _set_value_from_thumb_y(self, thumb_y: float, thumb_h: float) -> None:
        sb_min, sb_max = self._sb.minimum(), self._sb.maximum()
        sb_range = max(0, sb_max - sb_min)
        if sb_range <= 0:
            return
        travel = max(0.0, float(self.height()) - float(thumb_h))
        if travel <= 0.0:
            self._sb.setValue(int(sb_min))
            return
        progress = max(0.0, min(1.0, float(thumb_y) / travel))
        self._sb.setValue(int(round(sb_min + progress * float(sb_range))))

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: ARG002
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
        painter.setPen(QtCore.Qt.PenStyle.NoPen)
        radius = float(self.width()) / 2.0
        if self._track_color.alpha() > 0:
            painter.setBrush(self._track_color)
            painter.drawRoundedRect(QtCore.QRectF(self.rect()), radius, radius)
        painter.setBrush(self._thumb_color)
        painter.drawRoundedRect(self._compute_thumb(), radius, radius)

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        thumb = self._compute_thumb()
        if thumb.height() <= 0:
            return
        self._dragging = True
        self._drag_offset_y = (
            int(event.pos().y() - thumb.y()) if thumb.contains(event.position()) else int(thumb.height() / 2.0)
        )
        target = max(0.0, min(float(self.height()) - float(thumb.height()), float(event.pos().y() - self._drag_offset_y)))
        self._set_value_from_thumb_y(target, float(thumb.height()))
        event.accept()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        if not self._dragging:
            return
        thumb = self._compute_thumb()
        if thumb.height() <= 0:
            return
        target = max(0.0, min(float(self.height()) - float(thumb.height()), float(event.pos().y() - self._drag_offset_y)))
        self._set_value_from_thumb_y(target, float(thumb.height()))
        event.accept()

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        self._dragging = False
        event.accept()

    def wheelEvent(self, event: QtGui.QWheelEvent) -> None:
        QtWidgets.QApplication.sendEvent(self._sb, event)


class OverlayScrollArea(QtWidgets.QScrollArea):
    """Список с overlay-скроллбаром у правого края (как лента чата в I2PChat / macOS)."""

    _EDGE_MARGIN = 3

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        self.setWidgetResizable(True)
        self.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self._bar = RoundedVerticalScrollbar(self.verticalScrollBar(), self)
        self._bar.hide()
        self.verticalScrollBar().rangeChanged.connect(self._sync_bar)
        self.verticalScrollBar().valueChanged.connect(self._sync_bar)

    def apply_theme(self, theme: str) -> None:
        self._bar.apply_theme(theme)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._sync_bar()

    def _sync_bar(self, *_args: object) -> None:
        bar = self.verticalScrollBar()
        needed = bar.maximum() > 0
        self._bar.setVisible(needed)
        if not needed:
            return
        width = self._bar.width()
        self._bar.setGeometry(
            self.width() - width - self._EDGE_MARGIN,
            8,
            width,
            max(16, self.height() - 16),
        )
        self._bar.raise_()


class FramelessRoundedPopup(QtWidgets.QFrame):
    """Popup без нативной рамки: на Windows/Linux рисуем сами, на macOS достаточно QSS."""

    RADIUS = 12.0

    def __init__(self, object_name: str, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._paint_bg = sys.platform != "darwin"
        self._popup_bg = QtGui.QColor(246, 247, 250)
        self._popup_border = QtGui.QColor(208, 211, 218)
        self.setWindowFlags(popup_window_flags())
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self._dwm_patched = False
        self.setObjectName(object_name)

        root = QtWidgets.QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)
        self.surface = QtWidgets.QFrame(self)
        self.surface.setObjectName(object_name + "Surface")
        root.addWidget(self.surface)

    def _apply_linux_mask(self) -> None:
        if self._paint_bg and sys.platform.startswith("linux"):
            update_popup_rounded_mask(self, self.RADIUS)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        super().paintEvent(event)
        if self._paint_bg:
            paint_popup_rounded_bg(self, self._popup_bg, self._popup_border, self.RADIUS)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._apply_linux_mask()

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        super().showEvent(event)
        if not self._dwm_patched:
            disable_dwm_rounded_frame(self)
            self._dwm_patched = True
        if self._paint_bg:
            QtCore.QTimer.singleShot(0, self._apply_linux_mask)

    def set_popup_colors(self, *, night: bool) -> None:
        if night:
            self._popup_bg = QtGui.QColor(28, 31, 40, 250)
            self._popup_border = QtGui.QColor(58, 62, 74)
        else:
            self._popup_bg = QtGui.QColor(246, 247, 250)
            self._popup_border = QtGui.QColor(208, 211, 218)

    def shell_stylesheet(self, *, night: bool, extra: str = "") -> str:
        name = self.objectName()
        surface = name + "Surface"
        if self._paint_bg:
            return f"""
                #{name} {{ background: transparent; }}
                #{surface} {{
                    background: transparent;
                    border: none;
                    border-radius: {int(self.RADIUS)}px;
                }}
                {extra}
            """
        if night:
            bg, border = "rgba(28, 31, 40, 0.98)", "rgba(255, 255, 255, 0.14)"
        else:
            bg, border = "#f6f7fa", "rgba(0, 0, 0, 0.12)"
        return f"""
            #{name} {{ background: transparent; }}
            #{surface} {{
                background: {bg};
                border: 1px solid {border};
                border-radius: {int(self.RADIUS)}px;
            }}
            {extra}
        """


class _ComboItemDelegate(QtWidgets.QStyledItemDelegate):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._night = False

    def set_theme(self, night: bool) -> None:
        self._night = night

    def sizeHint(self, option: QtWidgets.QStyleOptionViewItem, index: QtCore.QModelIndex) -> QtCore.QSize:
        self.initStyleOption(option, index)
        height = QtGui.QFontMetrics(option.font).height() + 10
        return QtCore.QSize(super().sizeHint(option, index).width(), max(26, min(height, 36)))

    def paint(
        self,
        painter: QtGui.QPainter,
        option: QtWidgets.QStyleOptionViewItem,
        index: QtCore.QModelIndex,
    ) -> None:
        self.initStyleOption(option, index)
        opt = QtWidgets.QStyleOptionViewItem(option)
        opt.text = ""
        opt.icon = QtGui.QIcon()
        widget = opt.widget
        style = widget.style() if widget is not None else QtWidgets.QApplication.style()
        selected = bool(opt.state & QtWidgets.QStyle.StateFlag.State_Selected)
        hovered = bool(opt.state & QtWidgets.QStyle.StateFlag.State_MouseOver)
        base = QtWidgets.QStyleOptionViewItem(opt)
        base.state &= ~(QtWidgets.QStyle.StateFlag.State_Selected | QtWidgets.QStyle.StateFlag.State_MouseOver)
        painter.save()
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
        painter.setClipRect(base.rect)
        style.drawControl(QtWidgets.QStyle.ControlElement.CE_ItemViewItem, base, painter, widget)
        text = str(index.data(QtCore.Qt.ItemDataRole.DisplayRole) or "")
        if not text:
            painter.restore()
            return
        if self._night:
            sel_bg, hov_bg = QtGui.QColor("#3a5588"), QtGui.QColor("#2c3039")
            sel_fg, txt_fg = QtGui.QColor("#f4f7ff"), QtGui.QColor("#d8deea")
        else:
            sel_bg, hov_bg = QtGui.QColor("#dbe9ff"), QtGui.QColor("#e8eef8")
            sel_fg, txt_fg = QtGui.QColor("#1b4f9f"), QtGui.QColor("#2f3644")
        if selected or hovered:
            pill = opt.rect.adjusted(2, 1, -2, -1)
            if sys.platform == "darwin":
                pill.translate(0, -1)
            painter.setPen(QtCore.Qt.PenStyle.NoPen)
            painter.setBrush(sel_bg if selected else hov_bg)
            painter.drawRoundedRect(QtCore.QRectF(pill), 6.0, 6.0)
        painter.setPen(sel_fg if selected else txt_fg)
        painter.setFont(opt.font)
        shift = -2 if sys.platform == "darwin" else 0
        painter.drawText(
            opt.rect.adjusted(12, shift, -10, shift),
            int(QtCore.Qt.AlignmentFlag.AlignLeft | QtCore.Qt.AlignmentFlag.AlignVCenter),
            text,
        )
        painter.restore()


def _list_stylesheet(night: bool) -> str:
    color = "#d8deea" if night else "#2f3644"
    return f"""
        QListWidget#StyledComboPopupList {{
            background: transparent;
            border: none;
            outline: none;
            color: {color};
            font-size: 13px;
            padding: 0px;
        }}
        QListWidget#StyledComboPopupList::item {{
            border: none;
            padding: 0px;
            margin: 0px;
        }}
    """


class ComboPopup(FramelessRoundedPopup):
    itemChosen = QtCore.pyqtSignal(str)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__("StyledComboPopup", parent)
        inner = QtWidgets.QVBoxLayout(self.surface)
        inner.setContentsMargins(8, 8, 8, 8)
        inner.setSpacing(0)
        self.list = QtWidgets.QListWidget(self.surface)
        self.list.setObjectName("StyledComboPopupList")
        self.list.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        self.list.setSpacing(2)
        self.list.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.list.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.list.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollMode.ScrollPerPixel)
        self.list.itemClicked.connect(self._on_item_clicked)
        self.list.itemActivated.connect(self._on_item_clicked)
        self._delegate = _ComboItemDelegate(self.list)
        self.list.setItemDelegate(self._delegate)
        self.list.setUniformItemSizes(True)
        inner.addWidget(self.list)

    def _on_item_clicked(self, item: QtWidgets.QListWidgetItem) -> None:
        self.itemChosen.emit(item.text())
        self.hide()

    def set_items(self, values: list[str], selected_text: str) -> None:
        self.list.clear()
        selected_row = 0
        for index, value in enumerate(values):
            self.list.addItem(value)
            if value == selected_text:
                selected_row = index
        if values:
            self.list.setCurrentRow(selected_row)

    def _content_height(self) -> int:
        count = self.list.count()
        if count <= 0:
            return 8
        visible = min(count, 8)
        self.list.setFixedHeight(4096)
        self.list.doItemsLayout()
        first = self.list.item(0)
        last = self.list.item(visible - 1)
        if first is not None and last is not None:
            top = self.list.visualItemRect(first).top()
            bottom = self.list.visualItemRect(last).bottom()
            if bottom > top:
                return bottom - top + 2
        heights = []
        for index in range(visible):
            row = self.list.sizeHintForRow(index)
            heights.append(row if row > 0 else 28)
        return sum(heights) + self.list.spacing() * max(0, visible - 1) + 2

    def _fit_list_height(self) -> int:
        height = self._content_height()
        self.list.setFixedHeight(height)
        overflow = self.list.verticalScrollBar().maximum()
        if overflow > 0:
            height += overflow + 4
            self.list.setFixedHeight(height)
        return height

    def _size_to_anchor(self, anchor: QtWidgets.QWidget) -> tuple[int, int]:
        list_h = self._fit_list_height()
        margins = self.surface.layout().contentsMargins()
        width = max(anchor.width(), 160)
        height = margins.top() + list_h + margins.bottom()
        self.setFixedSize(width, height)
        return width, height

    def _resync_below(self, anchor: QtWidgets.QWidget) -> None:
        if not self.isVisible():
            return
        width, height = self._size_to_anchor(anchor)
        self.move(
            global_position_popup_below_anchor(anchor, width, height, vertical_gap=4, align_right=False)
        )

    def show_below(self, anchor: QtWidgets.QWidget) -> None:
        width, height = self._size_to_anchor(anchor)
        self.move(global_position_popup_below_anchor(anchor, width, height, vertical_gap=4, align_right=False))
        self.show()
        QtCore.QTimer.singleShot(0, lambda a=anchor: self._resync_below(a))
        QtCore.QTimer.singleShot(0, self.list.setFocus)

    def apply_theme(self, theme: str) -> None:
        night = theme == "night"
        self.set_popup_colors(night=night)
        self._delegate.set_theme(night)
        self.setStyleSheet(self.shell_stylesheet(night=night, extra=_list_stylesheet(night)))
        self.list.viewport().update()
        self.update()


class _ComboBox(QtWidgets.QComboBox):
    popupRequested = QtCore.pyqtSignal()

    def showPopup(self) -> None:
        self.popupRequested.emit()

    def hidePopup(self) -> None:
        return


class StyledCombo(QtWidgets.QWidget):
    """QComboBox с кастомным popup и стрелкой ∨ вместо нативного списка Qt."""

    currentIndexChanged = QtCore.pyqtSignal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding, QtWidgets.QSizePolicy.Policy.Preferred)
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.combo = _ComboBox(self)
        self.combo.setEditable(False)
        self.combo.setInsertPolicy(QtWidgets.QComboBox.InsertPolicy.NoInsert)
        self.combo.setMaxVisibleItems(8)
        self.combo.currentIndexChanged.connect(self.currentIndexChanged.emit)
        self.combo.popupRequested.connect(self._show_popup)
        layout.addWidget(self.combo)
        self._arrow = QtWidgets.QLabel("∨", self)
        self._arrow.setAlignment(QtCore.Qt.AlignmentFlag.AlignCenter)
        self._arrow.setAttribute(QtCore.Qt.WidgetAttribute.WA_TransparentForMouseEvents)
        self.popup = ComboPopup(self)
        self.popup.itemChosen.connect(self._on_item_chosen)
        self._theme = "light"
        self.apply_theme("light")

    def addItem(self, text: str, userData: Any = None) -> None:
        self.combo.addItem(text, userData)

    def clear(self) -> None:
        self.combo.clear()

    def findData(self, data: Any) -> int:
        return self.combo.findData(data)

    def currentData(self) -> Any:
        return self.combo.currentData()

    def currentIndex(self) -> int:
        return self.combo.currentIndex()

    def setCurrentIndex(self, index: int) -> None:
        self.combo.setCurrentIndex(index)

    def apply_theme(self, theme: str) -> None:
        self._theme = "night" if theme == "night" else "light"
        self._arrow.setStyleSheet(
            f"color: {_combo_arrow_color(self._theme)}; font-size: 10px; background: transparent;"
        )
        self.popup.apply_theme(self._theme)

    def _show_popup(self) -> None:
        values = [self.combo.itemText(index) for index in range(self.combo.count())]
        self.popup.set_items(values, self.combo.currentText())
        self.popup.show_below(self.combo)

    def _on_item_chosen(self, text: str) -> None:
        index = self.combo.findText(text)
        if index >= 0:
            self.combo.setCurrentIndex(index)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._arrow.setGeometry(self.width() - 28, 0, 28, self.height())
        if self.popup.isVisible():
            self.popup.show_below(self.combo)


class ActionsPopupButton(QtWidgets.QFrame):
    clicked = QtCore.pyqtSignal()

    def __init__(self, text: str, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setObjectName("ActionsPopupItem")
        self.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
        self.setCursor(QtCore.Qt.CursorShape.PointingHandCursor)
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(12, 8, 12, 8)
        self._title = QtWidgets.QLabel(text, self)
        self._title.setObjectName("ActionsPopupItemTitle")
        self._title.setAttribute(QtCore.Qt.WidgetAttribute.WA_TransparentForMouseEvents)
        layout.addWidget(self._title, 1)
        self.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)

    def apply_colors(self, *, night: bool) -> None:
        color = "#eceff4" if night else "#1d1d1f"
        disabled = "#8b93a5" if night else "#8e8e93"
        self._title.setStyleSheet(
            f"QLabel#ActionsPopupItemTitle {{ color: {color}; }}"
            f"QLabel#ActionsPopupItemTitle:disabled {{ color: {disabled}; }}"
        )

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() == QtCore.Qt.MouseButton.LeftButton and self.isEnabled() and self.rect().contains(event.pos()):
            self.clicked.emit()
            event.accept()
            return
        super().mouseReleaseEvent(event)


class ActionsPopup(FramelessRoundedPopup):
    RADIUS = 14.0
    closed = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__("ActionsPopup", parent)
        self.setMinimumWidth(236)
        self.surface_layout = QtWidgets.QVBoxLayout(self.surface)
        self.surface_layout.setContentsMargins(8, 8, 8, 8)
        self.surface_layout.setSpacing(4)
        self._night = False
        self.setFocusPolicy(QtCore.Qt.FocusPolicy.StrongFocus)

    def add_action(self, text: str, callback: Callable[[], None], enabled: bool = True) -> None:
        button = ActionsPopupButton(text, self.surface)
        button.setEnabled(enabled)
        button.setCursor(
            QtCore.Qt.CursorShape.PointingHandCursor
            if enabled
            else QtCore.Qt.CursorShape.ArrowCursor
        )
        button.apply_colors(night=self._night)
        button.clicked.connect(lambda: (self.hide(), callback()))
        self.surface_layout.addWidget(button)

    def add_separator(self) -> None:
        sep = QtWidgets.QFrame(self.surface)
        sep.setObjectName("ActionsPopupSeparator")
        sep.setFrameShape(QtWidgets.QFrame.Shape.HLine)
        sep.setFrameShadow(QtWidgets.QFrame.Shadow.Plain)
        self.surface_layout.addWidget(sep)

    def show_below(self, anchor: QtWidgets.QWidget) -> None:
        self.adjustSize()
        self.move(
            global_position_popup_below_anchor(
                anchor, self.width(), self.height(), vertical_gap=6, align_right=True
            )
        )
        self.show()
        QtCore.QTimer.singleShot(0, lambda: self.setFocus(QtCore.Qt.FocusReason.PopupFocusReason))

    def hideEvent(self, event: QtGui.QHideEvent) -> None:
        super().hideEvent(event)
        self.closed.emit()

    def keyPressEvent(self, event: QtGui.QKeyEvent) -> None:
        if event.key() == QtCore.Qt.Key.Key_Escape:
            self.hide()
            event.accept()
            return
        super().keyPressEvent(event)

    def apply_theme(self, theme: str) -> None:
        night = theme == "night"
        self._night = night
        self.set_popup_colors(night=night)
        if night:
            items = """
                QFrame#ActionsPopupItem { background: transparent; border: none; border-radius: 10px; }
                QFrame#ActionsPopupItem:hover { background: rgba(255, 255, 255, 0.10); }
                QFrame#ActionsPopupItem:disabled { background: transparent; }
                QFrame#ActionsPopupSeparator {
                    background: #343a46; max-height: 1px; min-height: 1px; border: none; margin: 4px 8px;
                }
            """
        else:
            items = """
                QFrame#ActionsPopupItem { background: transparent; border: none; border-radius: 10px; }
                QFrame#ActionsPopupItem:hover { background: #e5eaf2; }
                QFrame#ActionsPopupItem:disabled { background: transparent; }
                QFrame#ActionsPopupSeparator {
                    background: #d6dce7; max-height: 1px; min-height: 1px; border: none; margin: 4px 8px;
                }
            """
        self.setStyleSheet(self.shell_stylesheet(night=night, extra=items))
        for index in range(self.surface_layout.count()):
            widget = self.surface_layout.itemAt(index).widget()
            if isinstance(widget, ActionsPopupButton):
                widget.apply_colors(night=night)
        self.update()


class _SpinFocusFilter(QtCore.QObject):
    def __init__(self, row: QtWidgets.QFrame) -> None:
        super().__init__(row)
        self._row = row

    def eventFilter(self, obj: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if event.type() == QtCore.QEvent.Type.FocusIn:
            self._row.setProperty("focused", True)
        elif event.type() == QtCore.QEvent.Type.FocusOut:
            self._row.setProperty("focused", False)
        else:
            return False
        self._row.style().unpolish(self._row)
        self._row.style().polish(self._row)
        self._row.update()
        return False


def wrap_spin_row(spin: QtWidgets.QSpinBox) -> QtWidgets.QFrame:
    """QSpinBox без нативных кнопок: колонка ▲/▼ справа, как в I2PChat."""
    spin.setButtonSymbols(QtWidgets.QAbstractSpinBox.ButtonSymbols.NoButtons)
    frame = QtWidgets.QFrame()
    frame.setObjectName("SpinRow")
    frame.setFrameShape(QtWidgets.QFrame.Shape.NoFrame)
    frame.setProperty("focused", False)
    layout = QtWidgets.QHBoxLayout(frame)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.setSpacing(0)
    layout.addWidget(spin, 1)
    step_col = QtWidgets.QWidget()
    step_col.setObjectName("SpinStepColumn")
    steps = QtWidgets.QVBoxLayout(step_col)
    steps.setContentsMargins(0, 0, 0, 0)
    steps.setSpacing(0)
    up = QtWidgets.QToolButton(step_col)
    up.setObjectName("SpinStepUp")
    down = QtWidgets.QToolButton(step_col)
    down.setObjectName("SpinStepDown")
    for button, text in ((up, "▲"), (down, "▼")):
        button.setText(text)
        button.setAutoRaise(True)
        button.setToolButtonStyle(QtCore.Qt.ToolButtonStyle.ToolButtonTextOnly)
        button.setCursor(QtCore.Qt.CursorShape.PointingHandCursor)
        button.setAutoRepeat(True)
        button.setAutoRepeatDelay(400)
        button.setAutoRepeatInterval(120)
        font = button.font()
        font.setPixelSize(10)
        button.setFont(font)

    def step_up() -> None:
        spin.stepUp()
        spin.setFocus(QtCore.Qt.FocusReason.OtherFocusReason)

    def step_down() -> None:
        spin.stepDown()
        spin.setFocus(QtCore.Qt.FocusReason.OtherFocusReason)

    up.clicked.connect(step_up)
    down.clicked.connect(step_down)
    steps.addWidget(up)
    steps.addWidget(down)
    layout.addWidget(step_col, 0)
    spin.installEventFilter(_SpinFocusFilter(frame))
    return frame
