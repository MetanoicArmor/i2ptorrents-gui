use std::fs;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::i18n::normalize_language;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct AppSettings {
    pub rpc_url: String,
    pub torrents_dir: String,
    pub refresh_seconds: u32,
    pub theme: String,
    pub language: String,
    pub torrent_view: String,
    pub http_proxy: String,
}

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            rpc_url: "http://127.0.0.1:9191/mytorrents".to_string(),
            torrents_dir: default_torrents_dir().to_string_lossy().into_owned(),
            refresh_seconds: 5,
            theme: "light".to_string(),
            language: "en".to_string(),
            torrent_view: "detailed".to_string(),
            http_proxy: "socks5://127.0.0.1:4447".to_string(),
        }
    }
}

impl AppSettings {
    pub fn load_from(path: &Path) -> Self {
        let defaults = Self::default();
        let Ok(text) = fs::read_to_string(path) else {
            return defaults;
        };
        let Ok(data) = serde_json::from_str::<serde_json::Value>(&text) else {
            return defaults;
        };
        let Some(obj) = data.as_object() else {
            return defaults;
        };
        Self {
            rpc_url: json_string(obj.get("rpc_url"), &defaults.rpc_url),
            torrents_dir: json_string(
                obj.get("torrents_dir"),
                &default_torrents_dir().to_string_lossy(),
            ),
            refresh_seconds: obj
                .get("refresh_seconds")
                .and_then(|v| {
                    v.as_u64()
                        .or_else(|| v.as_str().and_then(|s| s.parse().ok()))
                })
                .map(|n| n as u32)
                .unwrap_or(5)
                .clamp(2, 60),
            theme: if json_string(obj.get("theme"), "") == "night" {
                "night".to_string()
            } else {
                "light".to_string()
            },
            language: normalize_language(
                obj.get("language")
                    .and_then(|v| v.as_str())
                    .or(Some(&defaults.language)),
            ),
            torrent_view: normalize_view(&json_string(
                obj.get("torrent_view"),
                &defaults.torrent_view,
            )),
            http_proxy: migrate_proxy(&json_string(obj.get("http_proxy"), &defaults.http_proxy)),
        }
    }

    pub fn load() -> Self {
        Self::load_from(&config_dir().join("settings.json"))
    }

    pub fn save_to(&self, path: &Path) -> std::io::Result<()> {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        let temporary = path.with_extension("tmp");
        fs::write(
            &temporary,
            serde_json::to_string_pretty(self).unwrap_or_else(|_| "{}".to_string()),
        )?;
        fs::rename(&temporary, path)
    }

    pub fn save(&self) -> std::io::Result<()> {
        self.save_to(&config_dir().join("settings.json"))
    }

    pub fn torrents_path(&mut self) -> std::io::Result<PathBuf> {
        let mut path = expand_user(&self.torrents_dir);
        if path.as_os_str().is_empty() {
            path = default_torrents_dir();
            self.torrents_dir = path.to_string_lossy().into_owned();
        }
        fs::create_dir_all(&path)?;
        Ok(path)
    }
}

pub fn config_dir() -> PathBuf {
    if cfg!(windows) {
        PathBuf::from(
            std::env::var("APPDATA").unwrap_or_else(|_| dirs_home().to_string_lossy().into_owned()),
        )
        .join("i2ptorrents-gui")
    } else if cfg!(target_os = "macos") {
        dirs_home().join("Library/Application Support/i2ptorrents-gui")
    } else {
        PathBuf::from(
            std::env::var("XDG_CONFIG_HOME")
                .unwrap_or_else(|_| dirs_home().join(".config").to_string_lossy().into_owned()),
        )
        .join("i2ptorrents-gui")
    }
}

fn default_torrents_dir() -> PathBuf {
    dirs_home().join("torrents")
}

fn dirs_home() -> PathBuf {
    std::env::var_os("HOME")
        .or_else(|| std::env::var_os("USERPROFILE"))
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."))
}

fn expand_user(value: &str) -> PathBuf {
    let raw = value.trim();
    if let Some(rest) = raw.strip_prefix("~/") {
        return dirs_home().join(rest);
    }
    if raw == "~" {
        return dirs_home();
    }
    PathBuf::from(raw)
}

fn json_string(value: Option<&serde_json::Value>, default: &str) -> String {
    match value {
        Some(serde_json::Value::String(text)) => text.clone(),
        Some(other) if !other.is_null() => other.to_string().trim_matches('"').to_string(),
        _ => default.to_string(),
    }
}

fn normalize_view(value: &str) -> String {
    let lowered = value.trim().to_lowercase();
    if matches!(
        lowered.as_str(),
        "simple" | "compact" | "упрощённый" | "упрощенный"
    ) {
        "simple".to_string()
    } else {
        "detailed".to_string()
    }
}

fn migrate_proxy(value: &str) -> String {
    let raw = value.trim();
    if raw.is_empty() {
        "socks5://127.0.0.1:4447".to_string()
    } else {
        raw.to_string()
    }
}
