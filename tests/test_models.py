from i2ptorrents.models import Torrent, TorrentStatus, format_bytes, format_rate


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
    })
    assert torrent.status is TorrentStatus.SEEDING
    assert torrent.finished
    assert torrent.progress == 1
    assert torrent.hash_string == "abc"


def test_progress_is_clamped() -> None:
    torrent = Torrent.from_rpc({"id": 1, "totalSize": 10, "leftUntilDone": 20})
    assert torrent.progress == 0


def test_human_readable_units() -> None:
    assert format_bytes(1024) == "1.0 КБ"
    assert format_rate(1024) == "1.0 КБ/с"
