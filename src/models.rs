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
        })
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
    value.and_then(Value::as_bool)
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
