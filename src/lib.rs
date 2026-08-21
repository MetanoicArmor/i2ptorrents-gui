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
    for root in roots {
        let Ok(resolved) = root.canonicalize() else {
            continue;
        };
        if !seen.insert(resolved.clone()) {
            continue;
        }
        let path = resolved.join(name);
        if path.is_file() {
            return Some(path);
        }
        let nested = resolved.join("assets").join(name);
        if nested.is_file() {
            return Some(nested);
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
