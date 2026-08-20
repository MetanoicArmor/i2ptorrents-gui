"""Скруглённые подсказки вместо нативного QTipLabel (как в I2PChat)."""

from __future__ import annotations

import sys
from typing import Callable, Optional

from PyQt6 import QtCore, QtGui, QtWidgets

from .popup_geometry import clamp_popup_top_left_to_available_geometry, disable_dwm_rounded_frame

_RADIUS_PX = 12.0
_MAX_LABEL_WIDTH = 440
_FALLBACK_FADE_MS = 150
_DISMISS_DEBOUNCE_MS = 300
_WAKEUP_EXTRA_MS = 500
_WAKEUP_EXTRA_DARWIN_MS = 350

_panel: Optional["RoundedTooltipWindow"] = None
_orig_show_text: Optional[Callable[..., None]] = None
_orig_hide_text: Optional[Callable[[], None]] = None
_current_owner: Optional[QtWidgets.QWidget] = None
_tooltip_filter: Optional["_TooltipInterceptFilter"] = None


def tooltip_palette_colors(theme: str) -> tuple[str, str]:
    if theme == "night":
        return "#22252d", "#e3e8f1"
    return "#f2f4f8", "#1d1d1f"


def apply_tooltip_palette(app: QtWidgets.QApplication, theme: str) -> None:
    bg_hex, fg_hex = tooltip_palette_colors(theme)
    bg = QtGui.QColor(bg_hex)
    fg = QtGui.QColor(fg_hex)
    pal = QtGui.QPalette(app.palette())
    for group in (
        QtGui.QPalette.ColorGroup.Active,
        QtGui.QPalette.ColorGroup.Inactive,
        QtGui.QPalette.ColorGroup.Disabled,
    ):
        pal.setColor(group, QtGui.QPalette.ColorRole.ToolTipBase, bg)
        pal.setColor(group, QtGui.QPalette.ColorRole.ToolTipText, fg)
    app.setPalette(pal)


def _panel_alive() -> Optional["RoundedTooltipWindow"]:
    global _panel
    if _panel is None:
        return None
    try:
        _panel.isVisible()
    except RuntimeError:
        _panel = None
        return None
    return _panel


def _outline_color(bg: QtGui.QColor, fg: QtGui.QColor) -> QtGui.QColor:
    mix = 0.18
    return QtGui.QColor(
        int(round(bg.red() * (1.0 - mix) + fg.red() * mix)),
        int(round(bg.green() * (1.0 - mix) + fg.green() * mix)),
        int(round(bg.blue() * (1.0 - mix) + fg.blue() * mix)),
    )


def _clamp_global_top_left(top_left: QtCore.QPoint, width: int, height: int) -> QtCore.QPoint:
    screen = QtGui.QGuiApplication.screenAt(top_left) or QtGui.QGuiApplication.primaryScreen()
    if screen is None:
        return top_left
    return clamp_popup_top_left_to_available_geometry(
        top_left, width, height, screen.availableGeometry()
    )


def _cursor_over_owner_or_tip(owner: Optional[QtWidgets.QWidget]) -> bool:
    widget = QtWidgets.QApplication.widgetAt(QtGui.QCursor.pos())
    if widget is None:
        return False
    panel = _panel_alive()
    if panel is not None and (widget is panel or panel.isAncestorOf(widget)):
        return True
    return owner is not None and (widget is owner or owner.isAncestorOf(widget))


def _fade_delays_ms(widget: QtWidgets.QWidget) -> tuple[int, int]:
    app = QtWidgets.QApplication.instance()
    if app is None:
        return 0, 0
    if not app.isEffectEnabled(QtCore.Qt.UIEffect.UI_FadeTooltip):
        if sys.platform == "darwin":
            return _FALLBACK_FADE_MS, _FALLBACK_FADE_MS
        return 0, 0
    return _FALLBACK_FADE_MS, _FALLBACK_FADE_MS


def tooltip_window_flags() -> QtCore.Qt.WindowType:
    flags = (
        QtCore.Qt.WindowType.ToolTip
        | QtCore.Qt.WindowType.FramelessWindowHint
        | QtCore.Qt.WindowType.WindowDoesNotAcceptFocus
        | QtCore.Qt.WindowType.WindowStaysOnTopHint
    )
    if sys.platform == "darwin" or sys.platform.startswith("win"):
        flags |= QtCore.Qt.WindowType.NoDropShadowWindowHint
    return flags


class RoundedTooltipWindow(QtWidgets.QWidget):
    def __init__(self) -> None:
        super().__init__(None, tooltip_window_flags())
        self.setAttribute(QtCore.Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.setAutoFillBackground(False)
        self.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        self._dwm_patched = False
        self._radius = _RADIUS_PX
        self._label = QtWidgets.QLabel(self)
        self._label.setWordWrap(True)
        self._label.setMaximumWidth(_MAX_LABEL_WIDTH)
        self._label.setTextInteractionFlags(QtCore.Qt.TextInteractionFlag.NoTextInteraction)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(0)
        layout.addWidget(self._label)
        self._hide_timer = QtCore.QTimer(self)
        self._hide_timer.setSingleShot(True)
        self._hide_timer.timeout.connect(self._start_fade_out)
        linear = QtCore.QEasingCurve(QtCore.QEasingCurve.Type.Linear)
        self._fade_in = QtCore.QPropertyAnimation(self, b"windowOpacity", self)
        self._fade_in.setEasingCurve(linear)
        self._fade_out = QtCore.QPropertyAnimation(self, b"windowOpacity", self)
        self._fade_out.setEasingCurve(linear)
        self._dismiss_debounce = QtCore.QTimer(self)
        self._dismiss_debounce.setSingleShot(True)
        self._dismiss_debounce.timeout.connect(self._start_fade_out)

    def _cancel_dismiss_debounce(self) -> None:
        self._dismiss_debounce.stop()

    def _schedule_dismiss_debounced(self) -> None:
        if not self._dismiss_debounce.isActive():
            self._dismiss_debounce.start(_DISMISS_DEBOUNCE_MS)

    def _stop_opacity_animations(self) -> None:
        self._fade_in.stop()
        self._fade_out.stop()
        try:
            self._fade_out.finished.disconnect(self._after_fade_out_hide)
        except TypeError:
            pass

    def _after_fade_out_hide(self) -> None:
        global _current_owner
        try:
            self._fade_out.finished.disconnect(self._after_fade_out_hide)
        except TypeError:
            pass
        if self.isVisible():
            QtWidgets.QWidget.hide(self)
        self.setWindowOpacity(1.0)
        _current_owner = None

    def _force_hide_immediate(self) -> None:
        global _current_owner
        self._cancel_dismiss_debounce()
        self._hide_timer.stop()
        self._stop_opacity_animations()
        self.setWindowOpacity(1.0)
        if self.isVisible():
            QtWidgets.QWidget.hide(self)
        _current_owner = None

    def _start_fade_out(self) -> None:
        self._cancel_dismiss_debounce()
        self._hide_timer.stop()
        if not self.isVisible():
            self.setWindowOpacity(1.0)
            return
        if self._fade_out.state() == QtCore.QAbstractAnimation.State.Running:
            return
        _, fade_out_ms = _fade_delays_ms(self)
        self._fade_in.stop()
        if fade_out_ms <= 0:
            self._force_hide_immediate()
            return
        current = float(self.windowOpacity())
        if current <= 0.01:
            self._force_hide_immediate()
            return
        try:
            self._fade_out.finished.disconnect(self._after_fade_out_hide)
        except TypeError:
            pass
        self._fade_out.finished.connect(self._after_fade_out_hide)
        self._fade_out.setDuration(int(fade_out_ms))
        self._fade_out.setStartValue(current)
        self._fade_out.setEndValue(0.0)
        self._fade_out.start()

    def _update_mask(self) -> None:
        if sys.platform.startswith("win"):
            return
        rect = self.rect()
        if rect.width() < 2 or rect.height() < 2:
            return
        path = QtGui.QPainterPath()
        path.addRoundedRect(QtCore.QRectF(rect), self._radius, self._radius)
        self.setMask(QtGui.QRegion(path.toFillPolygon().toPolygon()))

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: ARG002
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
        painter.setCompositionMode(QtGui.QPainter.CompositionMode.CompositionMode_Source)
        painter.fillRect(self.rect(), QtGui.QColor(0, 0, 0, 0))
        painter.setCompositionMode(QtGui.QPainter.CompositionMode.CompositionMode_SourceOver)
        rect = QtCore.QRectF(self.rect()).adjusted(1.0, 1.0, -1.0, -1.0)
        app = QtWidgets.QApplication.instance()
        pal = app.palette() if app else self.palette()
        bg = pal.color(QtGui.QPalette.ColorGroup.Active, QtGui.QPalette.ColorRole.ToolTipBase)
        fg = pal.color(QtGui.QPalette.ColorGroup.Active, QtGui.QPalette.ColorRole.ToolTipText)
        pen = QtGui.QPen(_outline_color(bg, fg))
        pen.setWidth(1)
        pen.setCosmetic(True)
        pen.setJoinStyle(QtCore.Qt.PenJoinStyle.RoundJoin)
        painter.setPen(pen)
        painter.setBrush(bg)
        painter.drawRoundedRect(rect, self._radius, self._radius)
        painter.end()

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._update_mask()

    def showEvent(self, event: QtGui.QShowEvent) -> None:
        super().showEvent(event)
        if not self._dwm_patched:
            disable_dwm_rounded_frame(self)
            self._dwm_patched = True
        QtCore.QTimer.singleShot(0, self._update_mask)

    def present(
        self,
        global_top_left: QtCore.QPoint,
        text: str,
        msec: int,
        *,
        owner: Optional[QtWidgets.QWidget] = None,
    ) -> None:
        app = QtWidgets.QApplication.instance()
        pal = app.palette() if app else self.palette()
        fg = pal.color(QtGui.QPalette.ColorGroup.Active, QtGui.QPalette.ColorRole.ToolTipText)
        self._label.setStyleSheet(
            f"color: {fg.name(QtGui.QColor.NameFormat.HexRgb)}; "
            "background: transparent; border: none; margin: 0; padding: 0;"
        )
        global _current_owner
        _current_owner = owner
        self._stop_opacity_animations()
        fade_in_ms, _ = _fade_delays_ms(self)
        self._label.setText(text)
        self.adjustSize()
        pos = _clamp_global_top_left(global_top_left, self.width(), self.height())
        self.move(pos)
        self._update_mask()
        self.setWindowOpacity(0.0 if fade_in_ms > 0 else 1.0)
        self.show()
        self.raise_()
        if fade_in_ms > 0:
            self._fade_in.setDuration(int(fade_in_ms))
            self._fade_in.setStartValue(0.0)
            self._fade_in.setEndValue(1.0)
            self._fade_in.start()
        self._cancel_dismiss_debounce()
        self._hide_timer.stop()
        self._hide_timer.start(msec if msec > 0 else 30000)


def _ensure_panel() -> RoundedTooltipWindow:
    global _panel
    if _panel_alive() is None:
        _panel = RoundedTooltipWindow()
    return _panel


def hide_rounded_tooltip(*, immediate: bool = False) -> None:
    global _current_owner
    _current_owner = None
    panel = _panel_alive()
    if panel is None:
        return
    if immediate:
        panel._force_hide_immediate()
    else:
        panel._start_fade_out()


def show_rounded_tooltip_at(
    global_pos: QtCore.QPoint,
    text: str,
    *,
    msec: int = -1,
    owner: Optional[QtWidgets.QWidget] = None,
) -> None:
    if not text.strip():
        hide_rounded_tooltip()
        return
    _ensure_panel().present(global_pos, text.strip(), msec, owner=owner)


_HIDE_ON_EVENTS = frozenset(
    (
        QtCore.QEvent.Type.Leave,
        QtCore.QEvent.Type.Hide,
        QtCore.QEvent.Type.Close,
        QtCore.QEvent.Type.MouseMove,
        QtCore.QEvent.Type.MouseButtonPress,
        QtCore.QEvent.Type.Wheel,
        QtCore.QEvent.Type.KeyPress,
        QtCore.QEvent.Type.FocusOut,
        QtCore.QEvent.Type.WindowDeactivate,
    )
)


def _handle_tooltip_dismiss_events(receiver: QtCore.QObject, event: QtCore.QEvent) -> None:
    panel = _panel_alive()
    if panel is None or not panel.isVisible():
        return
    et = event.type()
    if et not in _HIDE_ON_EVENTS:
        return
    owner = _current_owner
    if et in (
        QtCore.QEvent.Type.MouseButtonPress,
        QtCore.QEvent.Type.Wheel,
        QtCore.QEvent.Type.KeyPress,
        QtCore.QEvent.Type.FocusOut,
        QtCore.QEvent.Type.WindowDeactivate,
    ):
        panel._cancel_dismiss_debounce()
        hide_rounded_tooltip()
        return
    if et in (QtCore.QEvent.Type.MouseMove, QtCore.QEvent.Type.Leave):
        if _cursor_over_owner_or_tip(owner):
            panel._cancel_dismiss_debounce()
        else:
            panel._schedule_dismiss_debounced()
        return
    if et in (QtCore.QEvent.Type.Hide, QtCore.QEvent.Type.Close):
        if isinstance(receiver, QtWidgets.QWidget) and owner is not None:
            if receiver is owner or owner.isAncestorOf(receiver) or receiver.isAncestorOf(owner):
                panel._cancel_dismiss_debounce()
                hide_rounded_tooltip()
        elif owner is None:
            panel._cancel_dismiss_debounce()
            hide_rounded_tooltip()


class _TooltipInterceptFilter(QtCore.QObject):
    def eventFilter(self, obj: QtCore.QObject, event: QtCore.QEvent) -> bool:
        if event.type() == QtCore.QEvent.Type.ToolTip and isinstance(event, QtGui.QHelpEvent):
            if isinstance(obj, QtWidgets.QWidget):
                tip = (obj.toolTip() or "").strip()
                if tip:
                    _ensure_panel().present(event.globalPos(), tip, -1, owner=obj)
                    return True
                hide_rounded_tooltip()
                return True
        try:
            _handle_tooltip_dismiss_events(obj, event)
        except RuntimeError:
            pass
        return False


def _install_monkey_patch() -> None:
    global _orig_show_text, _orig_hide_text
    if _orig_show_text is not None:
        return

    def show_text(
        pos: QtCore.QPoint,
        text: Optional[str],
        widget: Optional[QtWidgets.QWidget] = None,
        rect: QtCore.QRect = QtCore.QRect(),
        msecShowTime: int = -1,
    ) -> None:
        if not text:
            hide_rounded_tooltip()
            return
        if widget is not None and not rect.isNull():
            bottom = widget.mapToGlobal(rect.bottomLeft())
            pos = QtCore.QPoint(bottom.x(), bottom.y() + 2)
        _ensure_panel().present(pos, text, msecShowTime, owner=widget)

    _orig_show_text = QtWidgets.QToolTip.showText
    _orig_hide_text = QtWidgets.QToolTip.hideText
    QtWidgets.QToolTip.showText = show_text  # type: ignore[assignment]
    QtWidgets.QToolTip.hideText = lambda: hide_rounded_tooltip()  # type: ignore[assignment]


class _TooltipWakeUpProxyStyle(QtWidgets.QProxyStyle):
    def __init__(self, base_style: QtWidgets.QStyle, extra_wakeup_ms: int) -> None:
        super().__init__(base_style)
        self._extra_wakeup_ms = max(0, extra_wakeup_ms)

    def styleHint(
        self,
        hint: QtWidgets.QStyle.StyleHint,
        option: Optional[QtWidgets.QStyleOption] = None,
        widget: Optional[QtWidgets.QWidget] = None,
        returnData: Optional[QtWidgets.QStyleHintReturn] = None,
    ) -> int:
        value = super().styleHint(hint, option, widget, returnData)
        if hint == QtWidgets.QStyle.StyleHint.SH_ToolTip_WakeUpDelay:
            return int(value) + self._extra_wakeup_ms
        return int(value)


def install_tooltip_wakeup_delay_style(app: QtWidgets.QApplication) -> None:
    current = app.style()
    if isinstance(current, _TooltipWakeUpProxyStyle):
        return
    extra = _WAKEUP_EXTRA_MS
    if sys.platform == "darwin":
        extra += _WAKEUP_EXTRA_DARWIN_MS
    app.setStyle(_TooltipWakeUpProxyStyle(current, extra))


def install_rounded_tooltips(app: QtWidgets.QApplication) -> None:
    global _tooltip_filter
    _install_monkey_patch()
    if _tooltip_filter is None:
        _tooltip_filter = _TooltipInterceptFilter(app)
        app.installEventFilter(_tooltip_filter)
