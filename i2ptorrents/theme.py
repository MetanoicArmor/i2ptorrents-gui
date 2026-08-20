LIGHT = """
QMainWindow, QDialog { background: #e6eaf2; }
QWidget { color: #1d1d1f; font-size: 13px; }
QFrame#Surface, QFrame#Sidebar {
    background: #f2f4f8; border: 1px solid #ffffff; border-radius: 14px;
}
QFrame#TorrentCard, QFrame#SummaryCard {
    background: #f8f9fc; border: none; border-radius: 12px;
}
QLabel#Title { font-size: 21px; font-weight: 650; }
QLabel#SectionTitle { color: #525966; font-size: 12px; font-weight: 600; }
QLabel#TorrentName { color: #1d1d1f; font-size: 14px; font-weight: 650; }
QLabel#Secondary, QLabel#StatusText { color: #626875; font-size: 12px; }
QLabel#StatusOnline { color: #245039; background: #d7ebdc; border-radius: 10px; padding: 6px 12px; }
QLabel#StatusOffline { color: #7c302c; background: #f2d8d7; border-radius: 10px; padding: 6px 12px; }
QPushButton, QToolButton {
    background: #f8f9fc; border: none; border-radius: 9px; padding: 8px 14px; color: #20232b;
}
QPushButton:hover, QToolButton:hover { background: #ffffff; }
QPushButton:pressed, QToolButton:pressed { background: #e4e9f2; }
QPushButton#Primary { background: #0a84ff; color: #ffffff; }
QPushButton#Primary:hover { background: #2d95ff; }
QPushButton#Danger { background: #e6ebf3; color: #b94b45; }
QPushButton#Filter { text-align: left; background: transparent; color: #525966; }
QPushButton#Filter:checked { background: #dfe7f3; color: #1d1d1f; font-weight: 600; }
QLineEdit, QSpinBox, QComboBox {
    background: #ffffff; border: none; border-radius: 8px; padding: 8px 12px;
}
QComboBox:hover { background: #f7f8fb; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid #0a84ff; }
QFrame#SpinRow {
    background: #ffffff; border: 1px solid transparent; border-radius: 8px;
}
QFrame#SpinRow[focused="true"] { border: 1px solid #0a84ff; }
QFrame#SpinRow QSpinBox {
    background: transparent; border: none; padding: 8px 10px; min-height: 22px;
    selection-background-color: #0a84ff; selection-color: #ffffff;
}
QFrame#SpinRow QSpinBox:focus { border: none; }
QWidget#SpinStepColumn {
    background: #eef1f7; border: none;
    border-left: 1px solid #d8dce6;
    border-top-right-radius: 7px; border-bottom-right-radius: 7px;
}
QToolButton#SpinStepUp, QToolButton#SpinStepDown {
    border: none; background: transparent; color: #3f4757;
    font-size: 10px; min-width: 24px; max-width: 24px; min-height: 14px; padding: 0px;
    border-radius: 0px;
}
QToolButton#SpinStepUp:hover, QToolButton#SpinStepDown:hover {
    background: #e4e9f2; color: #1d1d1f;
}
QToolButton#SpinStepUp:pressed, QToolButton#SpinStepDown:pressed { background: #d5dbe8; }
QComboBox::drop-down {
    subcontrol-origin: padding; subcontrol-position: center right;
    width: 28px; border: none; background: transparent;
}
QComboBox::down-arrow { image: none; width: 0; height: 0; }
QProgressBar { background: #d6d7dd; border: none; border-radius: 4px; height: 8px; text-align: center; }
QProgressBar::chunk { background: #0a84ff; border-radius: 4px; }
QMenu {
    background: #f6f7fa; color: #2c3442; border: none;
    padding: 8px; border-radius: 14px;
}
QMenu::item {
    background: transparent; color: #2c3442; padding: 9px 14px; border-radius: 9px;
}
QMenu::item:selected { background: #e5eaf2; }
QMenu::item:disabled { color: #8e8e93; }
QMenu::separator { height: 1px; background: #d6dce7; margin: 6px 8px; }
QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical {
    background: transparent; width: 6px; margin: 2px 0px 2px 0px;
}
QScrollBar::handle:vertical {
    background: rgba(60, 60, 67, 0.35); min-height: 24px; border-radius: 3px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QCheckBox { spacing: 8px; }
"""

NIGHT = """
QMainWindow, QDialog { background: #101013; }
QWidget { color: #f5f5f7; font-size: 13px; }
QFrame#Surface, QFrame#Sidebar {
    background: #191a1f; border: 1px solid #292b33; border-radius: 14px;
}
QFrame#TorrentCard, QFrame#SummaryCard { background: #22242b; border: none; border-radius: 12px; }
QLabel#Title { font-size: 21px; font-weight: 650; }
QLabel#SectionTitle { color: #9aa3b5; font-size: 12px; font-weight: 600; }
QLabel#TorrentName { color: #f5f5f7; font-size: 14px; font-weight: 650; }
QLabel#Secondary, QLabel#StatusText { color: #9aa3b5; font-size: 12px; }
QLabel#StatusOnline { color: #a9dfb9; background: #223c2b; border-radius: 10px; padding: 6px 12px; }
QLabel#StatusOffline { color: #f0aaa5; background: #472c2d; border-radius: 10px; padding: 6px 12px; }
QPushButton, QToolButton {
    background: #24262d; border: none; border-radius: 9px; padding: 8px 14px; color: #f5f5f7;
}
QPushButton:hover, QToolButton:hover { background: #30333c; }
QPushButton:pressed, QToolButton:pressed { background: #393d48; }
QPushButton#Primary { background: #0a84ff; color: #ffffff; }
QPushButton#Primary:hover { background: #2d95ff; }
QPushButton#Danger { background: #2a2d36; color: #f0aaa5; }
QPushButton#Filter { text-align: left; background: transparent; color: #a5adbd; }
QPushButton#Filter:checked { background: #293c5e; color: #ffffff; font-weight: 600; }
QLineEdit, QSpinBox, QComboBox {
    background: #1f1f23; border: none; border-radius: 8px; padding: 8px 12px;
}
QComboBox:hover { background: #242831; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border: 1px solid #0a84ff; }
QFrame#SpinRow {
    background: #1f1f23; border: 1px solid transparent; border-radius: 8px;
}
QFrame#SpinRow[focused="true"] { border: 1px solid #0a84ff; }
QFrame#SpinRow QSpinBox {
    background: transparent; border: none; padding: 8px 10px; min-height: 22px;
    selection-background-color: #0a84ff; selection-color: #ffffff;
}
QFrame#SpinRow QSpinBox:focus { border: none; }
QWidget#SpinStepColumn {
    background: #2a2d36; border: none;
    border-left: 1px solid #3d4450;
    border-top-right-radius: 7px; border-bottom-right-radius: 7px;
}
QToolButton#SpinStepUp, QToolButton#SpinStepDown {
    border: none; background: transparent; color: #c6cfdf;
    font-size: 10px; min-width: 24px; max-width: 24px; min-height: 14px; padding: 0px;
    border-radius: 0px;
}
QToolButton#SpinStepUp:hover, QToolButton#SpinStepDown:hover {
    background: #353945; color: #f5f5f7;
}
QToolButton#SpinStepUp:pressed, QToolButton#SpinStepDown:pressed { background: #404554; }
QComboBox::drop-down {
    subcontrol-origin: padding; subcontrol-position: center right;
    width: 28px; border: none; background: transparent;
}
QComboBox::down-arrow { image: none; width: 0; height: 0; }
QProgressBar { background: #3b3e48; border: none; border-radius: 4px; height: 8px; text-align: center; }
QProgressBar::chunk { background: #0a84ff; border-radius: 4px; }
QMenu {
    background: rgba(34, 37, 45, 0.96); color: #e3e8f1; border: none;
    padding: 8px; border-radius: 14px;
}
QMenu::item {
    background: transparent; color: #e3e8f1; padding: 9px 14px; border-radius: 9px;
}
QMenu::item:selected { background: rgba(255, 255, 255, 0.10); }
QMenu::item:disabled { color: #8b93a5; }
QMenu::separator { height: 1px; background: #343a46; margin: 6px 8px; }
QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical {
    background: transparent; width: 6px; margin: 2px 0px 2px 0px;
}
QScrollBar::handle:vertical {
    background: rgba(255, 255, 255, 0.20); min-height: 24px; border-radius: 3px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QCheckBox { spacing: 8px; }
"""


def stylesheet(theme: str) -> str:
    return NIGHT if theme == "night" else LIGHT
