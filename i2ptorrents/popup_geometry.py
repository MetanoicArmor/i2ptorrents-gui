"""Геометрия всплывающих окон и Windows DWM — как в I2PChat."""

from __future__ import annotations

import sys
from typing import Optional

from PyQt6 import QtCore, QtGui, QtWidgets


def disable_dwm_rounded_frame(widget: QtWidgets.QWidget) -> None:
    """Windows 11: убрать системную рамку/скругления DWM вокруг popup-окна.

    Без этого DWM рисует прямоугольную 1 px кайму и/или свои скругления,
    которые не совпадают с QPainter-рендером. На других ОС ничего не делает.
    """
    if not sys.platform.startswith("win"):
        return
    try:
        import ctypes

        hwnd = int(widget.winId())
        dwmapi = ctypes.WinDLL("dwmapi", use_last_error=True)
        pref = ctypes.c_int(1)  # DWMWCP_DONOTROUND
        dwmapi.DwmSetWindowAttribute(hwnd, 33, ctypes.byref(pref), ctypes.sizeof(pref))
    except Exception:
        pass


def popup_window_flags() -> QtCore.Qt.WindowType:
    flags = QtCore.Qt.WindowType.Popup | QtCore.Qt.WindowType.FramelessWindowHint
    if sys.platform.startswith("win"):
        flags |= QtCore.Qt.WindowType.NoDropShadowWindowHint
    return flags


def paint_popup_rounded_bg(
    widget: QtWidgets.QWidget,
    bg: QtGui.QColor,
    border: QtGui.QColor,
    radius: float,
) -> None:
    """Anti-aliased rounded background + 1 px border (Windows/Linux)."""
    painter = QtGui.QPainter(widget)
    painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
    painter.setCompositionMode(QtGui.QPainter.CompositionMode.CompositionMode_Source)
    painter.fillRect(widget.rect(), QtGui.QColor(0, 0, 0, 0))
    painter.setCompositionMode(QtGui.QPainter.CompositionMode.CompositionMode_SourceOver)
    rect = QtCore.QRectF(widget.rect()).adjusted(0.5, 0.5, -0.5, -0.5)
    painter.setPen(QtGui.QPen(border, 1.0))
    painter.setBrush(QtGui.QBrush(bg))
    painter.drawRoundedRect(rect, radius, radius)
    painter.end()


def update_popup_rounded_mask(widget: QtWidgets.QWidget, radius: float) -> None:
    """Integer-based mask (fallback for non-composited Linux desktops)."""
    width, height = widget.width(), widget.height()
    if width < 2 or height < 2:
        return
    path = QtGui.QPainterPath()
    path.addRoundedRect(QtCore.QRectF(0, 0, float(width), float(height)), radius, radius)
    widget.setMask(QtGui.QRegion(path.toFillPolygon().toPolygon()))


def popup_screen_for_anchor(anchor: QtWidgets.QWidget) -> Optional[QtGui.QScreen]:
    if anchor.width() > 0 and anchor.height() > 0:
        center = anchor.mapToGlobal(QtCore.QPoint(anchor.width() // 2, anchor.height() // 2))
        screen = QtGui.QGuiApplication.screenAt(center)
        if screen is not None:
            return screen
    for point in (
        anchor.mapToGlobal(QtCore.QPoint(0, 0)),
        anchor.mapToGlobal(QtCore.QPoint(max(0, anchor.width() - 1), max(0, anchor.height() - 1))),
    ):
        screen = QtGui.QGuiApplication.screenAt(point)
        if screen is not None:
            return screen
    return QtGui.QGuiApplication.primaryScreen()


def clamp_popup_top_left_to_available_geometry(
    top_left: QtCore.QPoint, popup_w: int, popup_h: int, geom: QtCore.QRect
) -> QtCore.QPoint:
    x = max(geom.left(), min(top_left.x(), geom.right() - popup_w + 1))
    y = max(geom.top(), min(top_left.y(), geom.bottom() - popup_h + 1))
    return QtCore.QPoint(x, y)


def global_position_popup_below_anchor(
    anchor: QtWidgets.QWidget,
    popup_w: int,
    popup_h: int,
    *,
    vertical_gap: int,
    align_right: bool,
) -> QtCore.QPoint:
    x_local = max(0, anchor.width() - popup_w) if align_right else 0
    pos_below = anchor.mapToGlobal(QtCore.QPoint(x_local, anchor.height() + vertical_gap))
    pos_above = anchor.mapToGlobal(QtCore.QPoint(x_local, -popup_h - vertical_gap))
    screen = popup_screen_for_anchor(anchor)
    if screen is None:
        return pos_below
    geom = screen.availableGeometry()
    if pos_below.y() > geom.bottom() - popup_h + 1 and pos_above.y() >= geom.top():
        pos = pos_above
    else:
        pos = pos_below
    return clamp_popup_top_left_to_available_geometry(pos, popup_w, popup_h, geom)
