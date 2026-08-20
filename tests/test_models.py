import base64

from i2ptorrents.i18n import set_language
from i2ptorrents.models import Torrent, TorrentStatus, decode_piece_bitfield, format_bytes, format_rate


def test_torrent_parses_snake_case_fields_returned_by_i2pd() -> None:
    torrent = Torrent.from_rpc({
        "id": 7,
        "name": "Linux ISO",
        "status": 6,
        "is_finished": True,
        "total_size": 1024,
        "left_until_done": 0,
        "rate_upload": 512,
        "hash_string": "abc",
        "piece_count": 8,
        "piece_size": 256,
        "pieces": base64.b64encode(bytes([0x81])).decode(),
    })
    assert torrent.status is TorrentStatus.SEEDING
    assert torrent.finished
    assert torrent.progress == 1
    assert torrent.hash_string == "abc"
    assert torrent.pieces[0] is True
    assert torrent.pieces[-1] is True
    assert torrent.magnet_uri().startswith("magnet:?xt=urn:btih:abc")


def test_decode_piece_bitfield_msb_first() -> None:
    raw = base64.b64encode(bytes([0x81])).decode()
    assert decode_piece_bitfield(raw, 8) == (True, False, False, False, False, False, False, True)


def test_finished_torrent_without_bitfield_is_complete() -> None:
    assert decode_piece_bitfield("", 4, finished=True) == (True, True, True, True)


def test_progress_is_clamped() -> None:
    torrent = Torrent.from_rpc({"id": 1, "totalSize": 10, "leftUntilDone": 20})
    assert torrent.progress == 0


def test_human_readable_units() -> None:
    set_language("en")
    assert format_bytes(1024) == "1.0 KB"
    assert format_rate(1024) == "1.0 KB/s"
    set_language("ru")
    assert format_bytes(1024) == "1.0 КБ"
    assert format_rate(1024) == "1.0 КБ/с"
    set_language("en")
