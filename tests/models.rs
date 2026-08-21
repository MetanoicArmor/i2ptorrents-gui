use base64::Engine;
use i2ptorrents_gui::i18n::set_language;
use i2ptorrents_gui::models::{
    decode_piece_bitfield, format_bytes, format_rate, FilePriority, Torrent, TorrentStatus,
};
use serde_json::json;

#[test]
fn torrent_parses_snake_case_fields_returned_by_i2pd() {
    let torrent = Torrent::from_rpc(&json!({
        "id": 7,
        "name": "Linux ISO",
        "status": 6,
        "is_finished": true,
        "total_size": 1024,
        "left_until_done": 0,
        "rate_upload": 512,
        "hash_string": "abc",
        "piece_count": 8,
        "piece_size": 256,
        "pieces": base64::engine::general_purpose::STANDARD.encode([0x81u8]),
    }))
    .unwrap();
    assert_eq!(torrent.status, TorrentStatus::Seeding);
    assert!(torrent.finished);
    assert_eq!(torrent.progress(), 1.0);
    assert_eq!(torrent.hash_string, "abc");
    assert_eq!(torrent.pieces[0], true);
    assert_eq!(*torrent.pieces.last().unwrap(), true);
}

#[test]
fn torrent_parses_i2pd_files_wanted_priorities() {
    let torrent = Torrent::from_rpc(&json!({
        "id": 2,
        "name": "Album",
        "files": [
            {"name": "disk/a.flac", "length": 1000, "bytes_completed": 250},
            {"name": "/abs/b.flac", "length": 4000, "bytesCompleted": 0}
        ],
        "wanted": [1, 0],
        "priorities": [1, -1]
    }))
    .unwrap();
    assert_eq!(torrent.files.len(), 2);
    assert_eq!(torrent.files[0].display_name(), "disk/a.flac");
    assert_eq!(torrent.files[0].kind(), FilePriority::High);
    assert_eq!(torrent.files[0].progress_label(), "25%");
    assert_eq!(torrent.files[1].display_name(), "b.flac");
    assert_eq!(torrent.files[1].kind(), FilePriority::Skip);
}

#[test]
fn decode_piece_bitfield_msb_first() {
    let raw = json!(base64::engine::general_purpose::STANDARD.encode([0x81u8]));
    assert_eq!(
        decode_piece_bitfield(Some(&raw), 8, false),
        vec![true, false, false, false, false, false, false, true]
    );
}

#[test]
fn finished_torrent_without_bitfield_is_complete() {
    assert_eq!(
        decode_piece_bitfield(Some(&json!("")), 4, true),
        vec![true, true, true, true]
    );
}

#[test]
fn progress_is_clamped() {
    let torrent =
        Torrent::from_rpc(&json!({"id": 1, "totalSize": 10, "leftUntilDone": 20})).unwrap();
    assert_eq!(torrent.progress(), 0.0);
}

#[test]
fn human_readable_units() {
    set_language("en");
    assert_eq!(format_bytes(1024), "1.0 KB");
    assert_eq!(format_rate(1024), "1.0 KB/s");
    set_language("ru");
    assert_eq!(format_bytes(1024), "1.0 КБ");
    assert_eq!(format_rate(1024), "1.0 КБ/с");
    set_language("en");
}
