pub fn stylesheet(theme: &str) -> String {
    let base = if theme == "night" {
        include_str!("../assets/theme/night.qss")
    } else {
        include_str!("../assets/theme/light.qss")
    };
    let overlay = if theme == "night" {
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

pub fn tooltip_palette_colors(theme: &str) -> (&'static str, &'static str) {
    if theme == "night" {
        ("#2c2c2e", "#f5f5f7")
    } else {
        ("#f2f2f7", "#1d1d1f")
    }
}
