use std::sync::{Arc, Mutex};

use base64::Engine;
use i2ptorrents_gui::rpc::{
    normalize_rpc_url, RpcError, RpcTransport, TransmissionRPC, FILE_FIELDS, FIELDS,
};
use serde_json::{json, Value};

struct MockTransport {
    handler: Box<dyn Fn(&str, Value) -> Value>,
    captured: Arc<Mutex<Option<(String, Value)>>>,
}

impl RpcTransport for MockTransport {
    fn post_json(&self, _url: &str, body: &[u8]) -> Result<Vec<u8>, RpcError> {
        let payload: Value = serde_json::from_slice(body).unwrap();
        let method = payload["method"].as_str().unwrap().to_string();
        let arguments = payload["arguments"].clone();
        *self.captured.lock().unwrap() = Some((method.clone(), arguments.clone()));
        let result = (self.handler)(&method, arguments);
        Ok(serde_json::to_vec(&json!({ "result": "success", "arguments": result })).unwrap())
    }
}

#[test]
fn normalize_rpc_url_completes_path() {
    for (value, expected) in [
        (
            "127.0.0.1:9191/mytorrents",
            "http://127.0.0.1:9191/mytorrents/rpc/",
        ),
        (
            "http://localhost:9191/mytorrents/",
            "http://localhost:9191/mytorrents/rpc/",
        ),
        (
            "http://localhost:9191/mytorrents/rpc",
            "http://localhost:9191/mytorrents/rpc/",
        ),
        ("http://localhost:9191", "http://localhost:9191/rpc/"),
    ] {
        assert_eq!(normalize_rpc_url(value).unwrap(), expected);
    }
}

#[test]
fn remote_rpc_is_rejected_because_i2pd_has_no_authentication() {
    let err = normalize_rpc_url("http://192.0.2.10:9191/mytorrents").unwrap_err();
    assert!(err.contains("local address") || err.contains("локальн"));
}

#[test]
fn get_torrents_accepts_i2pd_result_shape() {
    let transport = MockTransport {
        handler: Box::new(|_, _| {
            json!({
                "torrents": [{
                    "id": 4,
                    "name": "Example",
                    "status": 4,
                    "total_size": 100,
                    "left_until_done": 25,
                    "rate_download": 10,
                }]
            })
        }),
        captured: Arc::new(Mutex::new(None)),
    };
    let client = TransmissionRPC::with_transport("localhost:9191/mytorrents", transport).unwrap();
    let torrent = client.get_torrents(true).unwrap().remove(0);
    assert_eq!(torrent.name, "Example");
    assert!((torrent.progress() - 0.75).abs() < f64::EPSILON);
    assert!(FIELDS.contains(&"pieces"));
    assert!(!FIELDS.contains(&"files"));
}

#[test]
fn get_torrents_omits_pieces_in_simple_view() {
    let captured = Arc::new(Mutex::new(None));
    let transport = MockTransport {
        handler: Box::new(|_, _| json!({"torrents": []})),
        captured: captured.clone(),
    };
    let client = TransmissionRPC::with_transport("localhost:9191/mytorrents", transport).unwrap();
    client.get_torrents(false).unwrap();
    let (method, arguments) = captured.lock().unwrap().clone().unwrap();
    assert_eq!(method, "torrent-get");
    let fields: Vec<_> = arguments["fields"]
        .as_array()
        .unwrap()
        .iter()
        .filter_map(Value::as_str)
        .collect();
    assert!(!fields.contains(&"pieces"));
    assert!(fields.contains(&"hashString"));
    assert!(!fields.contains(&"files"));
    assert!(!fields.contains(&"wanted"));
    assert!(!fields.contains(&"priorities"));
}

#[test]
fn get_torrent_files_requests_one_torrent() {
    let captured = Arc::new(Mutex::new(None));
    let transport = MockTransport {
        handler: Box::new(|_, _| {
            json!({
                "torrents": [{
                    "id": 7,
                    "files": [{"name": "a.bin", "length": 10, "bytesCompleted": 3}],
                    "wanted": [1],
                    "priorities": [0]
                }]
            })
        }),
        captured: captured.clone(),
    };
    let client = TransmissionRPC::with_transport("localhost:9191/mytorrents", transport).unwrap();
    let files = client.get_torrent_files(7).unwrap();
    assert_eq!(files.len(), 1);
    assert_eq!(files[0].display_name(), "a.bin");
    let (method, arguments) = captured.lock().unwrap().clone().unwrap();
    assert_eq!(method, "torrent-get");
    assert_eq!(arguments["ids"], json!([7]));
    let fields: Vec<_> = arguments["fields"]
        .as_array()
        .unwrap()
        .iter()
        .filter_map(Value::as_str)
        .collect();
    for field in FILE_FIELDS {
        assert!(fields.contains(field));
    }
}

#[test]
fn get_torrents_accepts_jsonrpc2_result_object() {
    struct JsonRpc2Transport;
    impl RpcTransport for JsonRpc2Transport {
        fn post_json(&self, _url: &str, _body: &[u8]) -> Result<Vec<u8>, RpcError> {
            Ok(serde_json::to_vec(&json!({
                "jsonrpc": "2.0",
                "id": 1,
                "result": {
                    "torrents": [{
                        "id": 2,
                        "name": "JsonRpc",
                        "status": 6,
                        "totalSize": 50,
                        "leftUntilDone": 0
                    }]
                }
            }))
            .unwrap())
        }
    }
    let client =
        TransmissionRPC::with_transport("localhost:9191/mytorrents", JsonRpc2Transport).unwrap();
    let torrent = client.get_torrents(false).unwrap().remove(0);
    assert_eq!(torrent.name, "JsonRpc");
    assert!(torrent.finished || torrent.progress() >= 1.0);
}

#[test]
fn add_torrent_sends_base64() {
    let transport = MockTransport {
        handler: Box::new(|method, arguments| {
            assert_eq!(method, "torrent-add");
            let metainfo = arguments["metainfo"].as_str().unwrap();
            assert_eq!(
                base64::engine::general_purpose::STANDARD
                    .decode(metainfo)
                    .unwrap(),
                b"d4:infode"
            );
            json!({"torrent-added": {"id": 1}})
        }),
        captured: Arc::new(Mutex::new(None)),
    };
    let dir = tempfile::tempdir().unwrap();
    let torrent_file = dir.path().join("test.torrent");
    std::fs::write(&torrent_file, b"d4:infode").unwrap();
    let client = TransmissionRPC::with_transport("localhost:9191", transport).unwrap();
    assert_eq!(client.add_torrent_path(&torrent_file).unwrap()["id"], 1);
}

#[test]
fn set_file_priority_sends_transmission_torrent_set() {
    let captured = Arc::new(Mutex::new(None));
    let transport = MockTransport {
        handler: Box::new(|_, _| json!({})),
        captured: captured.clone(),
    };
    let client = TransmissionRPC::with_transport("localhost:9191", transport).unwrap();
    client.set_file_priority(3, 1, true, 1).unwrap();
    let (method, arguments) = captured.lock().unwrap().clone().unwrap();
    assert_eq!(method, "torrent-set");
    assert_eq!(arguments["ids"], json!([3]));
    assert_eq!(arguments["files-wanted"], json!([1]));
    assert_eq!(arguments["priority-high"], json!([1]));

    client.set_file_priority(3, 2, false, 0).unwrap();
    let (method, arguments) = captured.lock().unwrap().clone().unwrap();
    assert_eq!(method, "torrent-set");
    assert_eq!(arguments["files-unwanted"], json!([2]));
}
