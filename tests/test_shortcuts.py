import os

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

pytest.importorskip("PyQt6.QtGui", exc_type=ImportError)

from i2ptorrents.main import _native_shortcut, _tip_with_shortcut


def test_native_shortcut_keeps_ctrl_sequences() -> None:
    assert "T" in _native_shortcut("Ctrl+T")
    assert "," in _native_shortcut("Ctrl+,")
    assert "O" in _native_shortcut("Ctrl+O")
    assert "S" in _native_shortcut("Ctrl+S")


def test_tip_with_shortcut_includes_label() -> None:
    tip = _tip_with_shortcut("Settings", "Ctrl+S", "Ctrl+,")
    assert tip.startswith("Settings (")
    assert "S" in tip
