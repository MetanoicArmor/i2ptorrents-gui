from pathlib import Path

from i2ptorrents.settings import AppSettings


def test_settings_roundtrip(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    expected = AppSettings(
        rpc_url="http://127.0.0.1:9999/test",
        torrents_dir="/tmp/torrents",
        refresh_seconds=9,
        theme="night",
        language="en",
        torrent_view="simple",
        http_proxy="socks5://127.0.0.1:4447",
    )
    expected.save(path)
    assert AppSettings.load(path) == expected


def test_settings_recover_from_invalid_json(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    path.write_text("{", encoding="utf-8")
    assert AppSettings.load(path).rpc_url.startswith("http://127.0.0.1")


def test_settings_keeps_socks_proxy(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    path.write_text('{"http_proxy": "socks5://127.0.0.1:4447"}', encoding="utf-8")
    assert AppSettings.load(path).http_proxy == "socks5://127.0.0.1:4447"


def test_torrent_view_roundtrip(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    AppSettings(torrent_view="simple").save(path)
    assert AppSettings.load(path).torrent_view == "simple"


def test_torrent_view_normalizes_aliases(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    path.write_text('{"torrent_view": "упрощённый"}', encoding="utf-8")
    assert AppSettings.load(path).torrent_view == "simple"
    path.write_text('{"torrent_view": "compact"}', encoding="utf-8")
    assert AppSettings.load(path).torrent_view == "simple"
    path.write_text('{"torrent_view": "full"}', encoding="utf-8")
    assert AppSettings.load(path).torrent_view == "detailed"


def test_torrents_path_creates_directory(tmp_path: Path) -> None:
    folder = tmp_path / "downloads" / "torrents"
    settings = AppSettings(torrents_dir=str(folder))
    assert settings.torrents_path() == folder
    assert folder.is_dir()
