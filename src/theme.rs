pub fn stylesheet(theme: &str) -> String {
    let base = if theme == "night" {
        include_str!("../assets/theme/night.qss")
    } else {
        include_str!("../assets/theme/light.qss")
    };
    let overlay = if cfg!(target_os = "linux") {
        linux_overlay(theme)
    } else if cfg!(target_os = "windows") {
        windows_overlay(theme)
    } else if theme == "night" {
        r#"
QMainWindow, QWidget#MainWindow { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #1c1c1e;
    border: none;
}
QWidget#PaneSplit { background: rgba(255, 255, 255, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
"#
    } else {
        r#"
QMainWindow, QWidget#MainWindow { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #ffffff;
    border: none;
}
QWidget#PaneSplit { background: rgba(0, 0, 0, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
"#
    };
    format!("{base}{overlay}{}", apple_font_overlay())
}

const APPLE_FONT_OVERLAY: &str = r#"
QWidget {
    font-family: "Inter", "SF Pro Text", "SF Pro Display", sans-serif;
}
"#;

fn apple_font_overlay() -> &'static str {
    if cfg!(any(target_os = "windows", target_os = "linux")) {
        APPLE_FONT_OVERLAY
    } else {
        ""
    }
}

fn windows_overlay(theme: &str) -> &'static str {
    if theme == "night" {
        r#"
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: transparent;
    border: none;
}
QWidget#PaneSplit { background: #3d3d3f; }
QFrame#TorrentCard, QFrame#SummaryCard, QWidget#TorrentCard, QWidget#SummaryCard {
    background: transparent;
    border: none;
    border-radius: 10px;
}
QWidget#FilesTablePane {
    background: transparent;
    border: none;
    border-radius: 12px;
}
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable {
    background: transparent;
    border: none;
    border-radius: 12px;
    gridline-color: transparent;
    outline: 0;
    alternate-background-color: #3d3d40;
}
QTableWidget#FilesTable::viewport {
    background: transparent;
    border: none;
    border-bottom-left-radius: 12px;
    border-bottom-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView {
    background: transparent;
    border: none;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section {
    background: #252527;
    color: #c7c7cc;
    border: none;
    border-bottom: 1px solid #48484a;
    padding: 8px 8px;
    font-weight: 600;
    font-size: 12px;
}
QTableWidget#FilesTable QHeaderView::section:first {
    border-top-left-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section:last {
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QScrollBar:horizontal { height: 0px; max-height: 0px; }
QTableWidget#FilesTable QScrollBar:vertical { width: 0px; max-width: 0px; }
QComboBoxPrivateContainer {
    background: transparent;
    border: none;
    padding: 0px;
}
QComboBox QAbstractItemView {
    background: transparent;
    border: none;
    outline: none;
}
"#
    } else {
        r#"
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: transparent;
    border: none;
}
QWidget#PaneSplit { background: #d8d8dc; }
QFrame#TorrentCard, QFrame#SummaryCard, QWidget#TorrentCard, QWidget#SummaryCard {
    background: transparent;
    border: none;
    border-radius: 10px;
}
QWidget#FilesTablePane {
    background: transparent;
    border: none;
    border-radius: 12px;
}
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable {
    background: transparent;
    border: none;
    border-radius: 12px;
    gridline-color: transparent;
    outline: 0;
    alternate-background-color: #e4e4e9;
}
QTableWidget#FilesTable::viewport {
    background: transparent;
    border: none;
    border-bottom-left-radius: 12px;
    border-bottom-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView {
    background: transparent;
    border: none;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section {
    background: #ececf1;
    color: #6e6e73;
    border: none;
    border-bottom: 1px solid #d0d0d5;
    padding: 8px 8px;
    font-weight: 600;
    font-size: 12px;
}
QTableWidget#FilesTable QHeaderView::section:first {
    border-top-left-radius: 12px;
}
QTableWidget#FilesTable QHeaderView::section:last {
    border-top-right-radius: 12px;
}
QTableWidget#FilesTable QScrollBar:horizontal { height: 0px; max-height: 0px; }
QTableWidget#FilesTable QScrollBar:vertical { width: 0px; max-width: 0px; }
QDialog QLineEdit, QDialog QSpinBox, QDialog QComboBox {
    background: #ffffff;
    border: none;
}
QDialog QComboBox:hover { background: #fafafc; }
QDialog QFrame#SpinRow, QDialog QFrame#PathRow {
    background: #ffffff;
    border: none;
}
QDialog QFrame#SpinRow[focused="true"] { border: 1px solid #0a84ff; }
QDialog QPushButton {
    background: #ffffff;
    border: none;
}
QDialog QPushButton:hover { background: #fafafc; }
QDialog QPushButton:pressed { background: #ececf1; }
QDialog QPushButton#Primary {
    background: #0a84ff;
    color: #ffffff;
}
QDialog QPushButton#Primary:hover { background: #2d95ff; }
QComboBoxPrivateContainer {
    background: transparent;
    border: none;
    padding: 0px;
}
QComboBox QAbstractItemView {
    background: transparent;
    border: none;
    outline: none;
}
"#
    }
}

fn linux_overlay(theme: &str) -> &'static str {
    if theme == "night" {
        r#"
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #1c1c1e;
    border: none;
}
QWidget#PaneSplit { background: rgba(255, 255, 255, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
QWidget#FilesTablePane { background: transparent; border: none; border-radius: 0px; }
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable { border-radius: 0px; outline: none; }
QTableWidget#FilesTable::viewport { border-radius: 0px; background: transparent; }
QTableWidget#FilesTable QHeaderView::section { border-radius: 0px; }
"#
    } else {
        r#"
QMainWindow, QWidget#MainWindow { background: transparent; }
QDialog { background: transparent; }
QFrame#Sidebar, QWidget#Sidebar {
    background: transparent;
    border: none;
}
QFrame#Surface, QWidget#Surface {
    background: #ffffff;
    border: none;
}
QWidget#PaneSplit { background: rgba(0, 0, 0, 0.10); }
QFrame#TorrentCard, QWidget#TorrentCard { background: transparent; border: none; }
QWidget#FilesTablePane { background: transparent; border: none; border-radius: 0px; }
QWidget#FilesTableOverlay { background: transparent; border: none; }
QTableWidget#FilesTable { border-radius: 0px; outline: none; }
QTableWidget#FilesTable::viewport { border-radius: 0px; background: transparent; }
QTableWidget#FilesTable QHeaderView::section { border-radius: 0px; }
"#
    }
}

pub fn tooltip_palette_colors(theme: &str) -> (&'static str, &'static str) {
    if theme == "night" {
        ("#2c2c2e", "#f5f5f7")
    } else {
        ("#f2f2f7", "#1d1d1f")
    }
}
