use i2ptorrents_gui::config::AppSettings;

#[test]
fn settings_roundtrip() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    let expected = AppSettings {
        rpc_url: "http://127.0.0.1:9999/test".into(),
        torrents_dir: "/tmp/torrents".into(),
        refresh_seconds: 9,
        theme: "night".into(),
        language: "en".into(),
        torrent_view: "simple".into(),
        http_proxy: "socks5://127.0.0.1:4447".into(),
        window_width: 1080,
        window_height: 700,
    };
    expected.save_to(&path).unwrap();
    assert_eq!(AppSettings::load_from(&path), expected);
}

#[test]
fn settings_recover_from_invalid_json() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    std::fs::write(&path, "{").unwrap();
    assert!(AppSettings::load_from(&path)
        .rpc_url
        .starts_with("http://127.0.0.1"));
}

#[test]
fn settings_keeps_socks_proxy() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    std::fs::write(&path, r#"{"http_proxy": "socks5://127.0.0.1:4447"}"#).unwrap();
    assert_eq!(
        AppSettings::load_from(&path).http_proxy,
        "socks5://127.0.0.1:4447"
    );
}

#[test]
fn torrent_view_roundtrip() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    AppSettings {
        torrent_view: "simple".into(),
        ..AppSettings::default()
    }
    .save_to(&path)
    .unwrap();
    assert_eq!(AppSettings::load_from(&path).torrent_view, "simple");
}

#[test]
fn torrent_view_normalizes_aliases() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    std::fs::write(&path, r#"{"torrent_view": "упрощённый"}"#).unwrap();
    assert_eq!(AppSettings::load_from(&path).torrent_view, "simple");
    std::fs::write(&path, r#"{"torrent_view": "compact"}"#).unwrap();
    assert_eq!(AppSettings::load_from(&path).torrent_view, "simple");
    std::fs::write(&path, r#"{"torrent_view": "full"}"#).unwrap();
    assert_eq!(AppSettings::load_from(&path).torrent_view, "detailed");
}

#[test]
fn torrents_path_creates_directory() {
    let dir = tempfile::tempdir().unwrap();
    let folder = dir.path().join("downloads").join("torrents");
    let mut settings = AppSettings {
        torrents_dir: folder.to_string_lossy().into_owned(),
        ..AppSettings::default()
    };
    assert_eq!(settings.torrents_path().unwrap(), folder);
    assert!(folder.is_dir());
}

#[test]
fn window_size_roundtrip() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    AppSettings {
        window_width: 1440,
        window_height: 900,
        ..AppSettings::default()
    }
    .save_to(&path)
    .unwrap();
    let loaded = AppSettings::load_from(&path);
    assert_eq!(loaded.window_width, 1440);
    assert_eq!(loaded.window_height, 900);
}

#[test]
fn window_size_defaults_when_missing() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    std::fs::write(&path, r#"{"language": "ru"}"#).unwrap();
    let loaded = AppSettings::load_from(&path);
    assert_eq!(loaded.window_width, 1080);
    assert_eq!(loaded.window_height, 700);
}

#[test]
fn window_size_clamps_minimum() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("settings.json");
    std::fs::write(&path, r#"{"window_width": 100, "window_height": 50}"#).unwrap();
    let loaded = AppSettings::load_from(&path);
    assert_eq!(loaded.window_width, 780);
    assert_eq!(loaded.window_height, 520);
}
