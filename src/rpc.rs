use std::net::IpAddr;
use std::time::Duration;

use serde_json::{json, Value};
use url::Url;

use crate::i18n::{t, t_args};
use crate::models::Torrent;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RpcError(pub String);

impl std::fmt::Display for RpcError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for RpcError {}

pub const FIELDS: &[&str] = &[
    "id",
    "name",
    "status",
    "isFinished",
    "sizeWhenDone",
    "leftUntilDone",
    "rateDownload",
    "rateUpload",
    "peersGettingFromUs",
    "peersSendingToUs",
    "pieceCount",
    "pieceSize",
    "totalSize",
    "hashString",
    "pieces",
    "files",
    "wanted",
    "priorities",
];

const MAX_RESPONSE_BYTES: usize = 8 * 1024 * 1024;

pub fn normalize_rpc_url(value: &str) -> Result<String, String> {
    let mut raw = value.trim().to_string();
    if raw.is_empty() {
        return Err(t("rpc_url_required"));
    }
    if !raw.contains("://") {
        raw = format!("http://{raw}");
    }
    let parsed = Url::parse(&raw).map_err(|_| t("rpc_url_invalid"))?;
    if parsed.scheme() != "http" && parsed.scheme() != "https" {
        return Err(t("rpc_url_invalid"));
    }
    let host = parsed
        .host_str()
        .ok_or_else(|| t("rpc_url_invalid"))?
        .trim_end_matches('.')
        .to_lowercase();
    if host != "localhost" {
        let ip: IpAddr = host.parse().map_err(|_| t("rpc_url_local_only"))?;
        if !ip.is_loopback() {
            return Err(t("rpc_url_local_only"));
        }
    }
    let mut path = parsed.path().trim_end_matches('/').to_string();
    if !path.ends_with("/rpc") {
        path.push_str("/rpc");
    }
    path.push('/');
    Ok(format!(
        "{}://{}{}",
        parsed.scheme(),
        parsed.host_str().unwrap_or("127.0.0.1"),
        if let Some(port) = parsed.port() {
            format!(":{port}{path}")
        } else {
            path
        }
    ))
}

pub trait RpcTransport {
    fn post_json(&self, url: &str, body: &[u8]) -> Result<Vec<u8>, RpcError>;
}

pub struct UreqTransport {
    timeout: Duration,
}

impl Default for UreqTransport {
    fn default() -> Self {
        Self {
            timeout: Duration::from_secs(5),
        }
    }
}

impl RpcTransport for UreqTransport {
    fn post_json(&self, url: &str, body: &[u8]) -> Result<Vec<u8>, RpcError> {
        let agent = ureq::builder()
            .timeout_connect(self.timeout)
            .timeout_read(self.timeout)
            .redirects(0)
            .build();
        let response = agent
            .post(url)
            .set("Content-Type", "application/json")
            .set("Accept", "application/json")
            .set("User-Agent", "i2ptorrents-gui/0.1")
            .send_bytes(body)
            .map_err(|err| match err {
                ureq::Error::Status(code, resp) => {
                    let detail = resp.into_string().unwrap_or_default();
                    RpcError(t_args(
                        "rpc_http",
                        &[("code", &code.to_string()), ("detail", detail.trim())],
                    ))
                }
                other => RpcError(t_args(
                    "rpc_no_connection",
                    &[("reason", &other.to_string())],
                )),
            })?;
        let mut raw = Vec::new();
        response
            .into_reader()
            .take((MAX_RESPONSE_BYTES + 1) as u64)
            .read_to_end(&mut raw)
            .map_err(|err| {
                RpcError(t_args("rpc_no_connection", &[("reason", &err.to_string())]))
            })?;
        if raw.len() > MAX_RESPONSE_BYTES {
            return Err(RpcError(t("rpc_too_large")));
        }
        Ok(raw)
    }
}

pub struct TransmissionRPC<T: RpcTransport = UreqTransport> {
    pub endpoint: String,
    transport: T,
    tag: std::cell::Cell<u64>,
}

impl TransmissionRPC<UreqTransport> {
    pub fn new(endpoint: &str) -> Result<Self, String> {
        Ok(Self {
            endpoint: normalize_rpc_url(endpoint)?,
            transport: UreqTransport::default(),
            tag: std::cell::Cell::new(0),
        })
    }
}

impl<T: RpcTransport> TransmissionRPC<T> {
    pub fn with_transport(endpoint: &str, transport: T) -> Result<Self, String> {
        Ok(Self {
            endpoint: normalize_rpc_url(endpoint)?,
            transport,
            tag: std::cell::Cell::new(0),
        })
    }

    pub fn call(&self, method: &str, arguments: Value) -> Result<Value, RpcError> {
        let tag = self.tag.get() + 1;
        self.tag.set(tag);
        let body = serde_json::to_vec(&json!({
            "method": method,
            "arguments": arguments,
            "tag": tag,
        }))
        .map_err(|_| RpcError(t("rpc_bad_response")))?;
        let raw = self.transport.post_json(&self.endpoint, &body)?;
        let payload: Value =
            serde_json::from_slice(&raw).map_err(|_| RpcError(t("rpc_bad_response")))?;
        let obj = payload
            .as_object()
            .ok_or_else(|| RpcError(t("rpc_bad_format")))?;
        if let Some(error) = obj.get("error") {
            let message = error
                .get("message")
                .and_then(Value::as_str)
                .map(str::to_string)
                .unwrap_or_else(|| {
                    if error.is_object() {
                        t("rpc_unknown_error")
                    } else {
                        error.to_string()
                    }
                });
            return Err(RpcError(t_args("rpc_error", &[("message", &message)])));
        }
        if let Some(result) = obj.get("result") {
            if !result.is_null() && result.as_str() != Some("success") && !result.is_object() {
                return Err(RpcError(t_args(
                    "rpc_error",
                    &[(
                        "message",
                        &result.as_str().unwrap_or(&result.to_string()).to_string(),
                    )],
                )));
            }
        }
        let result = obj
            .get("arguments")
            .cloned()
            .or_else(|| obj.get("result").cloned())
            .unwrap_or(json!({}));
        Ok(if result.is_object() {
            result
        } else {
            json!({})
        })
    }

    pub fn get_torrents(&self, detailed: bool) -> Result<Vec<Torrent>, RpcError> {
        let fields: Vec<&str> = if detailed {
            FIELDS.to_vec()
        } else {
            FIELDS
                .iter()
                .copied()
                .filter(|field| *field != "pieces")
                .collect()
        };
        let result = self.call("torrent-get", json!({ "fields": fields }))?;
        let rows = result
            .get("torrents")
            .and_then(Value::as_array)
            .ok_or_else(|| RpcError(t("rpc_bad_list")))?;
        Ok(rows.iter().filter_map(Torrent::from_rpc).collect())
    }

    pub fn add_torrent_bytes(&self, content: &[u8]) -> Result<Value, RpcError> {
        if content.is_empty() {
            return Err(RpcError(t("rpc_empty_file")));
        }
        let metainfo = base64::Engine::encode(&base64::engine::general_purpose::STANDARD, content);
        let result = self.call("torrent-add", json!({ "metainfo": metainfo }))?;
        Ok(result
            .get("torrent-added")
            .cloned()
            .or_else(|| result.get("torrent-duplicate").cloned())
            .filter(Value::is_object)
            .unwrap_or(json!({})))
    }

    pub fn add_torrent_path(&self, path: &std::path::Path) -> Result<Value, RpcError> {
        let content = std::fs::read(path)
            .map_err(|err| RpcError(t_args("rpc_read_failed", &[("error", &err.to_string())])))?;
        self.add_torrent_bytes(&content)
    }

    pub fn remove_torrent(&self, torrent_id: i64, delete_data: bool) -> Result<(), RpcError> {
        self.call(
            "torrent-remove",
            json!({ "ids": [torrent_id], "delete-local-data": delete_data }),
        )?;
        Ok(())
    }

    pub fn set_file_priority(
        &self,
        torrent_id: i64,
        index: i64,
        wanted: bool,
        priority: i64,
    ) -> Result<(), RpcError> {
        let mut arguments = json!({ "ids": [torrent_id] });
        if wanted {
            arguments["files-wanted"] = json!([index]);
            let key = match priority {
                -1 => "priority-low",
                1 => "priority-high",
                _ => "priority-normal",
            };
            arguments[key] = json!([index]);
        } else {
            arguments["files-unwanted"] = json!([index]);
        }
        self.call("torrent-set", arguments)?;
        Ok(())
    }
}

pub fn rpc_method_unsupported(message: &str) -> bool {
    let lower = message.to_ascii_lowercase();
    lower.contains("method not found") || lower.contains("-32601")
}

use std::io::Read;
