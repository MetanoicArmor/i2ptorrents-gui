import base64
import json
from pathlib import Path

import pytest

from i2ptorrents.rpc import TransmissionRPC, normalize_rpc_url


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("127.0.0.1:9191/mytorrents", "http://127.0.0.1:9191/mytorrents/rpc/"),
        ("http://localhost:9191/mytorrents/", "http://localhost:9191/mytorrents/rpc/"),
        ("http://localhost:9191/mytorrents/rpc", "http://localhost:9191/mytorrents/rpc/"),
        ("http://localhost:9191", "http://localhost:9191/rpc/"),
    ],
)
def test_normalize_rpc_url(value: str, expected: str) -> None:
    assert normalize_rpc_url(value) == expected


def test_get_torrents_accepts_i2pd_result_shape(monkeypatch: pytest.MonkeyPatch) -> None:
    client = TransmissionRPC("localhost:9191/mytorrents")
    monkeypatch.setattr(
        client,
        "_call",
        lambda method, args: {
            "torrents": [{
                "id": 4,
                "name": "Example",
                "status": 4,
                "total_size": 100,
                "left_until_done": 25,
                "rate_download": 10,
            }]
        },
    )
    torrent = client.get_torrents()[0]
    assert torrent.name == "Example"
    assert torrent.progress == 0.75


def test_add_torrent_sends_base64(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    torrent_file = tmp_path / "test.torrent"
    torrent_file.write_bytes(b"d4:infode")
    client = TransmissionRPC("localhost:9191")
    captured = {}

    def call(method: str, arguments: dict) -> dict:
        captured.update(method=method, arguments=arguments)
        return {"torrent-added": {"id": 1}}

    monkeypatch.setattr(client, "_call", call)
    assert client.add_torrent(torrent_file)["id"] == 1
    assert captured["method"] == "torrent-add"
    assert base64.b64decode(captured["arguments"]["metainfo"]) == b"d4:infode"
