from __future__ import annotations

import os
import sys

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

pytest.importorskip("PyQt6.QtWidgets", exc_type=ImportError)

from PyQt6 import QtCore, QtWidgets

from i2ptorrents.popup_geometry import (
    clamp_popup_top_left_to_available_geometry,
    disable_dwm_rounded_frame,
    popup_window_flags,
)
from i2ptorrents.rounded_tooltip import tooltip_palette_colors, tooltip_window_flags
from i2ptorrents.styled_popups import ComboPopup, StyledCombo


@pytest.fixture
def qapp() -> QtWidgets.QApplication:
    app = QtWidgets.QApplication.instance()
    if app is None:
        app = QtWidgets.QApplication([])
    return app


def test_clamp_popup_stays_inside_geometry() -> None:
    geom = QtCore.QRect(0, 0, 200, 100)
    pos = clamp_popup_top_left_to_available_geometry(QtCore.QPoint(180, 90), 40, 30, geom)
    assert pos.x() == 160
    assert pos.y() == 70


def test_disable_dwm_is_safe_off_windows(qapp: QtWidgets.QApplication) -> None:
    widget = QtWidgets.QWidget()
    disable_dwm_rounded_frame(widget)


def test_popup_flags_drop_native_shadow_on_windows(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(sys, "platform", "win32")
    flags = popup_window_flags()
    assert flags & QtCore.Qt.WindowType.Popup
    assert flags & QtCore.Qt.WindowType.FramelessWindowHint
    assert flags & QtCore.Qt.WindowType.NoDropShadowWindowHint


def test_popup_flags_keep_shadow_off_windows(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(sys, "platform", "linux")
    flags = popup_window_flags()
    assert flags & QtCore.Qt.WindowType.Popup
    assert not flags & QtCore.Qt.WindowType.NoDropShadowWindowHint


def test_tooltip_flags_drop_shadow_on_windows_and_macos(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(sys, "platform", "win32")
    assert tooltip_window_flags() & QtCore.Qt.WindowType.NoDropShadowWindowHint
    monkeypatch.setattr(sys, "platform", "darwin")
    assert tooltip_window_flags() & QtCore.Qt.WindowType.NoDropShadowWindowHint
    monkeypatch.setattr(sys, "platform", "linux")
    assert not tooltip_window_flags() & QtCore.Qt.WindowType.NoDropShadowWindowHint


def test_tooltip_palette_follows_theme() -> None:
    assert tooltip_palette_colors("night") == ("#22252d", "#e3e8f1")
    assert tooltip_palette_colors("light") == ("#f2f4f8", "#1d1d1f")


def test_overlay_scroll_hides_native_bar(qapp: QtWidgets.QApplication) -> None:
    from i2ptorrents.styled_popups import OverlayScrollArea

    area = OverlayScrollArea()
    inner = QtWidgets.QWidget()
    inner.setMinimumSize(180, 800)
    area.setWidget(inner)
    area.resize(200, 120)
    area.show()
    qapp.processEvents()
    area.verticalScrollBar().setRange(0, 400)
    area._sync_bar()
    assert area.verticalScrollBarPolicy() == QtCore.Qt.ScrollBarPolicy.ScrollBarAlwaysOff
    assert area._bar.isVisible()
    assert area._bar.x() == area.width() - area._bar.width() - OverlayScrollArea._EDGE_MARGIN


def test_styled_combo_keeps_user_data(qapp: QtWidgets.QApplication) -> None:
    combo = StyledCombo()
    combo.addItem("English", "en")
    combo.addItem("Русский", "ru")
    combo.setCurrentIndex(1)
    assert combo.currentData() == "ru"
    assert combo.findData("en") == 0


def test_spin_row_steps_value(qapp: QtWidgets.QApplication) -> None:
    from i2ptorrents.styled_popups import wrap_spin_row

    spin = QtWidgets.QSpinBox()
    spin.setRange(2, 60)
    spin.setValue(5)
    row = wrap_spin_row(spin)
    up = row.findChild(QtWidgets.QToolButton, "SpinStepUp")
    down = row.findChild(QtWidgets.QToolButton, "SpinStepDown")
    assert up is not None and down is not None
    assert spin.buttonSymbols() == QtWidgets.QAbstractSpinBox.ButtonSymbols.NoButtons
    up.click()
    assert spin.value() == 6
    down.click()
    assert spin.value() == 5


def test_combo_popup_fits_two_items_without_scroll(qapp: QtWidgets.QApplication) -> None:
    host = QtWidgets.QWidget()
    host.resize(280, 80)
    host.show()
    popup = ComboPopup()
    popup.apply_theme("night")
    popup.set_items(["Light", "Night"], "Night")
    popup.show_below(host)
    qapp.processEvents()
    popup._resync_below(host)
    qapp.processEvents()
    first = popup.list.visualItemRect(popup.list.item(0))
    last = popup.list.visualItemRect(popup.list.item(1))
    assert last.bottom() <= popup.list.viewport().height()
    assert first.top() >= 0
    assert popup.list.verticalScrollBar().maximum() == 0
