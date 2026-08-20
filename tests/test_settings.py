from pathlib import Path

from i2ptorrents.settings import AppSettings


def test_settings_roundtrip(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    expected = AppSettings(
        rpc_url="http://127.0.0.1:9999/test",
        torrents_dir="/tmp/torrents",
        refresh_seconds=9,
        theme="night",
    )
    expected.save(path)
    assert AppSettings.load(path) == expected


def test_settings_recover_from_invalid_json(tmp_path: Path) -> None:
    path = tmp_path / "settings.json"
    path.write_text("{", encoding="utf-8")
    assert AppSettings.load(path).rpc_url.startswith("http://127.0.0.1")
