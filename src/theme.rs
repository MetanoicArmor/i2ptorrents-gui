pub fn stylesheet(theme: &str) -> String {
    let base = if theme == "night" {
        include_str!("../assets/theme/night.qss")
    } else {
        include_str!("../assets/theme/light.qss")
    };
    let overlay = if cfg!(target_os = "linux") {
        linux_overlay(theme)
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
    format!("{base}{overlay}")
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
