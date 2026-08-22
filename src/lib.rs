//! Core logic for the i2pd torrents GUI: RPC, settings, i18n.

pub mod config;
#[cfg(feature = "gui")]
pub mod gui;
pub mod i18n;
pub mod models;
pub mod piece_map;
#[cfg(feature = "gui")]
pub mod qt_chrome;
pub mod rpc;
pub mod theme;

pub const APP_NAME: &str = "I2P Torrents";
pub const APP_AUTHOR: &str = "Vade";
pub const APP_LICENSE: &str = "BSD-3-Clause";
pub const APP_GITHUB: &str = "https://github.com/MetanoicArmor/i2ptorrents-gui";
pub const APP_TON_ADDRESS: &str = "UQCsX_UVKylmlxb4dWZlXdmlyRzNm-kzUx7Ld1VQHk1ob0MY";

pub fn version() -> &'static str {
    const RAW: &str = include_str!("../VERSION");
    match RAW.trim() {
        "" => "0.1.0",
        value => value,
    }
}

pub fn resource_path(name: &str) -> Option<std::path::PathBuf> {
    resource_roots()
        .into_iter()
        .find_map(|root| resource_in_root(&root, name))
}

fn resource_in_root(root: &std::path::Path, name: &str) -> Option<std::path::PathBuf> {
    let path = root.join(name);
    if path.is_file() {
        return Some(path);
    }
    let nested = root.join("assets").join(name);
    if nested.is_file() {
        return Some(nested);
    }
    None
}

fn resource_roots() -> Vec<std::path::PathBuf> {
    let mut roots = Vec::new();
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            roots.push(dir.to_path_buf());
            roots.push(dir.join("../Resources"));
            if let Some(parent) = dir.parent() {
                roots.push(parent.to_path_buf());
            }
        }
    }
    if let Ok(cwd) = std::env::current_dir() {
        roots.push(cwd);
    }
    roots.push(std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")));
    let mut seen = std::collections::HashSet::new();
    roots
        .into_iter()
        .filter_map(|root| root.canonicalize().ok())
        .filter(|root| seen.insert(root.clone()))
        .collect()
}

pub fn resource_fonts_dir() -> Option<std::path::PathBuf> {
    for root in resource_roots() {
        for sub in ["fonts", "assets/fonts"] {
            let dir = root.join(sub);
            if !dir.is_dir() {
                continue;
            }
            let has_font = std::fs::read_dir(&dir).ok()?.flatten().any(|entry| {
                matches!(
                    entry.path().extension().and_then(|ext| ext.to_str()),
                    Some("otf" | "ttf" | "OTF" | "TTF")
                )
            });
            if has_font {
                return Some(dir);
            }
        }
    }
    None
}

pub fn application_icon_path() -> Option<std::path::PathBuf> {
    for name in ["icon.png", "I2PTorrents.ico", "image.png"] {
        if let Some(path) = resource_path(name) {
            return Some(path);
        }
    }
    None
}
