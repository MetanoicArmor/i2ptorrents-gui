use std::path::Path;

use crate::i18n::{t, t_args};
use serde_json::Value;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TorrentStatus {
    Stopped = 0,
    QueuedVerify = 1,
    Verifying = 2,
    QueuedDownload = 3,
    Downloading = 4,
    QueuedSeed = 5,
    Seeding = 6,
}

impl TorrentStatus {
    pub fn from_rpc(value: i64) -> Self {
        match value {
            1 => Self::QueuedVerify,
            2 => Self::Verifying,
            3 => Self::QueuedDownload,
            4 => Self::Downloading,
            5 => Self::QueuedSeed,
            6 => Self::Seeding,
            _ => Self::Stopped,
        }
    }

    pub fn label(self) -> String {
        t(match self {
            Self::Stopped => "status_stopped",
            Self::QueuedVerify => "status_queued_verify",
            Self::Verifying => "status_verifying",
            Self::QueuedDownload => "status_queued_download",
            Self::Downloading => "status_downloading",
            Self::QueuedSeed => "status_queued_seed",
            Self::Seeding => "status_seeding",
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FilePriority {
    Skip,
    Low,
    Normal,
    High,
}

impl FilePriority {
    pub fn from_rpc(wanted: bool, priority: i64) -> Self {
        if !wanted {
            return Self::Skip;
        }
        match priority {
            -1 => Self::Low,
            1 => Self::High,
            _ => Self::Normal,
        }
    }

    pub fn wanted(self) -> bool {
        self != Self::Skip
    }

    pub fn rpc_priority(self) -> i64 {
        match self {
            Self::Skip | Self::Normal => 0,
            Self::Low => -1,
            Self::High => 1,
        }
    }

    pub fn combo_index(self) -> i32 {
        match self {
            Self::Skip => 0,
            Self::Low => 1,
            Self::Normal => 2,
            Self::High => 3,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TorrentFile {
    pub index: i64,
    pub name: String,
    pub length: u64,
    pub bytes_completed: u64,
    pub wanted: bool,
    pub priority: i64,
}

impl TorrentFile {
    pub fn kind(&self) -> FilePriority {
        FilePriority::from_rpc(self.wanted, self.priority)
    }

    pub fn display_name(&self) -> String {
        display_file_name(&self.name)
    }

    pub fn progress_label(&self) -> String {
        if self.length == 0 {
            return "—".into();
        }
        format!(
            "{:.0}%",
            (self.bytes_completed as f64 / self.length as f64 * 100.0).clamp(0.0, 100.0)
        )
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct Torrent {
    pub id: i64,
    pub name: String,
    pub status: TorrentStatus,
    pub total_size: u64,
    pub left_until_done: u64,
    pub rate_download: u64,
    pub rate_upload: u64,
    pub peers_sending_to_us: u64,
    pub peers_getting_from_us: u64,
    pub piece_count: u64,
    pub piece_size: u64,
    pub hash_string: String,
    pub finished: bool,
    pub pieces: Vec<bool>,
    pub files: Vec<TorrentFile>,
}

impl Torrent {
    pub fn completed(&self) -> u64 {
        self.total_size.saturating_sub(self.left_until_done)
    }

    pub fn progress(&self) -> f64 {
        if self.total_size == 0 {
            return if self.finished { 1.0 } else { 0.0 };
        }
        (self.completed() as f64 / self.total_size as f64).clamp(0.0, 1.0)
    }

    pub fn short_hash(&self) -> String {
        let digest = self.hash_string.trim();
        if digest.len() <= 16 {
            digest.to_string()
        } else {
            format!("{}…{}", &digest[..10], &digest[digest.len() - 6..])
        }
    }

    pub fn from_rpc(data: &Value) -> Option<Self> {
        let obj = data.as_object()?;
        let id = json_i64(obj.get("id"))?;
        let status = TorrentStatus::from_rpc(json_i64(obj.get("status")).unwrap_or(0));
        let total = json_u64(value_of(obj, "totalSize", "total_size"))
            .or_else(|| json_u64(value_of(obj, "sizeWhenDone", "size_when_done")))
            .unwrap_or(0);
        let piece_count = json_u64(value_of(obj, "pieceCount", "piece_count")).unwrap_or(0);
        let finished = json_bool(value_of(obj, "isFinished", "is_finished")).unwrap_or(false);
        let hash = json_str(value_of(obj, "hashString", "hash_string"))
            .unwrap_or_default()
            .trim()
            .to_string();
        Some(Self {
            id,
            name: json_str(obj.get("name"))
                .filter(|s| !s.is_empty())
                .unwrap_or_else(|| t("untitled")),
            status,
            total_size: total,
            left_until_done: json_u64(value_of(obj, "leftUntilDone", "left_until_done"))
                .unwrap_or(0),
            rate_download: json_u64(value_of(obj, "rateDownload", "rate_download")).unwrap_or(0),
            rate_upload: json_u64(value_of(obj, "rateUpload", "rate_upload")).unwrap_or(0),
            peers_sending_to_us: json_u64(value_of(obj, "peersSendingToUs", "peers_sending_to_us"))
                .unwrap_or(0),
            peers_getting_from_us: json_u64(value_of(
                obj,
                "peersGettingFromUs",
                "peers_getting_from_us",
            ))
            .unwrap_or(0),
            piece_count,
            piece_size: json_u64(value_of(obj, "pieceSize", "piece_size")).unwrap_or(0),
            hash_string: hash,
            finished,
            pieces: decode_piece_bitfield(value_of(obj, "pieces", "pieces"), piece_count, finished),
            files: parse_torrent_files(obj),
        })
    }
}

pub fn parse_torrent_files(obj: &serde_json::Map<String, Value>) -> Vec<TorrentFile> {
    let rows = match value_of(obj, "files", "files").and_then(Value::as_array) {
        Some(items) if !items.is_empty() => items,
        _ => return Vec::new(),
    };
    let wanted = value_of(obj, "wanted", "wanted")
        .and_then(Value::as_array)
        .cloned()
        .unwrap_or_default();
    let priorities = value_of(obj, "priorities", "priorities")
        .and_then(Value::as_array)
        .cloned()
        .unwrap_or_default();
    rows.iter()
        .enumerate()
        .filter_map(|(index, item)| {
            let file = item.as_object()?;
            let name = json_str(value_of(file, "name", "name")).unwrap_or_default();
            Some(TorrentFile {
                index: index as i64,
                name,
                length: json_u64(value_of(file, "length", "length")).unwrap_or(0),
                bytes_completed: json_u64(value_of(file, "bytesCompleted", "bytes_completed"))
                    .unwrap_or(0),
                wanted: json_truthy(wanted.get(index)).unwrap_or(true),
                priority: json_i64(priorities.get(index)).unwrap_or(0),
            })
        })
        .collect()
}

pub fn display_file_name(name: &str) -> String {
    let path = Path::new(name);
    if path.is_absolute() || name.starts_with('/') || name.starts_with('\\') {
        path.file_name()
            .map(|part| part.to_string_lossy().into_owned())
            .filter(|part| !part.is_empty())
            .unwrap_or_else(|| name.to_string())
    } else {
        name.replace('\\', "/")
    }
}

fn value_of<'a>(
    obj: &'a serde_json::Map<String, Value>,
    camel: &str,
    snake: &str,
) -> Option<&'a Value> {
    obj.get(camel).or_else(|| obj.get(snake))
}

fn json_i64(value: Option<&Value>) -> Option<i64> {
    value.and_then(|v| v.as_i64().or_else(|| v.as_u64().map(|n| n as i64)))
}

fn json_u64(value: Option<&Value>) -> Option<u64> {
    value.and_then(|v| {
        v.as_u64()
            .or_else(|| v.as_i64().and_then(|n| u64::try_from(n).ok()))
    })
}

fn json_bool(value: Option<&Value>) -> Option<bool> {
    json_truthy(value)
}

fn json_truthy(value: Option<&Value>) -> Option<bool> {
    value.and_then(|v| {
        v.as_bool()
            .or_else(|| v.as_i64().map(|n| n != 0))
            .or_else(|| v.as_u64().map(|n| n != 0))
    })
}

fn json_str(value: Option<&Value>) -> Option<String> {
    value.and_then(|v| {
        v.as_str().map(str::to_string).or_else(|| {
            if v.is_null() {
                None
            } else {
                Some(v.to_string().trim_matches('"').to_string())
            }
        })
    })
}

pub fn decode_piece_bitfield(raw: Option<&Value>, piece_count: u64, finished: bool) -> Vec<bool> {
    let count = piece_count as usize;
    if count == 0 {
        return Vec::new();
    }
    let data = match raw {
        Some(Value::String(text)) => {
            let text = text.trim();
            if text.is_empty() {
                return if finished {
                    vec![true; count]
                } else {
                    Vec::new()
                };
            }
            match base64::Engine::decode(&base64::engine::general_purpose::STANDARD, text) {
                Ok(bytes) => bytes,
                Err(_) => return Vec::new(),
            }
        }
        Some(Value::Array(items)) => items
            .iter()
            .filter_map(|v| v.as_u64().map(|n| n as u8))
            .collect(),
        _ => {
            return if finished {
                vec![true; count]
            } else {
                Vec::new()
            }
        }
    };
    (0..count)
        .map(|index| {
            let offset = index / 8;
            let bit = 0x80 >> (index % 8);
            offset < data.len() && data[offset] & bit != 0
        })
        .collect()
}

pub fn format_bytes(value: u64) -> String {
    let mut size = value as f64;
    let units = ["unit_b", "unit_kb", "unit_mb", "unit_gb", "unit_tb"];
    for (index, key) in units.iter().enumerate() {
        if size < 1024.0 || index == units.len() - 1 {
            let label = t(key);
            return if *key == "unit_b" {
                format!("{size:.0} {label}")
            } else {
                format!("{size:.1} {label}")
            };
        }
        size /= 1024.0;
    }
    format!("{size:.1} {}", t("unit_tb"))
}

pub fn format_rate(value: u64) -> String {
    format!("{}{}", format_bytes(value), t("per_second"))
}

pub fn progress_text(torrent: &Torrent) -> String {
    let percent = format!("{:.1}", torrent.progress() * 100.0);
    let done = format_bytes(torrent.completed());
    let total = format_bytes(torrent.total_size);
    t_args(
        "progress_of",
        &[("percent", &percent), ("done", &done), ("total", &total)],
    )
}
