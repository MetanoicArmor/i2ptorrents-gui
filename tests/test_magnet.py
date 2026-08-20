from urllib.request import Request

from i2ptorrents.magnet import (
    download_torrent_from_magnet,
    extract_download_id,
    extract_formtoken,
    is_proxy_transport_error,
    MagnetError,
    normalize_http_proxy,
    parse_magnet,
    proxy_candidates,
    retry_after_seconds,
    tracker_web_url,
)
from i2ptorrents.rpc import RPCError


SAMPLE_MAGNET = (
    "magnet:?xt=urn:btih:f6fee9fb500807cdbff9683e9136470c25717b5a"
    "&dn=Narcissists.Playbook+%282026%29%5B1080p%5D"
    "&tr=http://tracker2.postman.i2p/announce.php"
)

HEAVY_LOAD = """
<title>PaTracker 1.7.5 | Site Under Heavy Load</title>
<form action="/?action=Enter" method="POST">
<input type="hidden" name="formtoken" value="28a4409df2eb580494d2f2d3eae2a7672a1189b0">
<input type="submit" value="Proceed">
</form>
"""

SEARCH_HIT = """
<table id="torrentView">
<tr>
<td><a href="index.php?action=Download&id=42">get</a></td>
<td>f6fee9fb500807cdbff9683e9136470c25717b5a Narcissists.Playbook</td>
</tr>
</table>
"""

TORRENT_BYTES = b"d8:announce32:http://tracker2.postman.i2p/a4:infod6:lengthi1eee"


class QueueOpener:
    def __init__(self, bodies: list[bytes]) -> None:
        self.bodies = list(bodies)
        self.requests: list[Request] = []

    def open(self, request: Request, timeout: float = 0):  # noqa: ARG002
        self.requests.append(request)
        body = self.bodies.pop(0)
        if body == b"HTTP_408":
            from io import BytesIO
            from urllib.error import HTTPError

            raise HTTPError(request.full_url, 408, "Request Timeout", None, BytesIO())
        if body == b"HTTP_503":
            from io import BytesIO
            from urllib.error import HTTPError

            raise HTTPError(
                request.full_url,
                503,
                "Service unavailable. throttling!",
                None,
                BytesIO(b"503 Service unavailable - Ratelimit active. Please wait at least 60 seconds and retry."),
            )

        class _Response:
            def read(self, size: int = -1) -> bytes:
                return body if size < 0 else body[:size]

            def __enter__(self):
                return self

            def __exit__(self, *args) -> bool:
                return False

        return _Response()


def test_parse_user_magnet() -> None:
    magnet = parse_magnet(SAMPLE_MAGNET)
    assert magnet.info_hash == "f6fee9fb500807cdbff9683e9136470c25717b5a"
    assert magnet.name == "Narcissists.Playbook (2026)[1080p]"
    assert magnet.trackers == ("http://tracker2.postman.i2p/announce.php",)
    assert tracker_web_url(magnet) == "http://tracker2.postman.i2p"


def test_parse_base32_btih() -> None:
    magnet = parse_magnet("magnet:?xt=urn:btih:AZQKE54LKE6Q3UTZ6W3LQN2J4Y7QZ4WA")
    assert len(magnet.info_hash) == 40


def test_parse_rejects_non_magnet() -> None:
    try:
        parse_magnet("/tmp/file.torrent")
    except RPCError as exc:
        assert "не magnet" in str(exc).lower() or "magnet" in str(exc).lower()
    else:
        raise AssertionError("expected MagnetError")


def test_proxy_candidates_prefer_socks() -> None:
    assert proxy_candidates("socks5://127.0.0.1:4447") == [
        "socks5://127.0.0.1:4447",
        "http://127.0.0.1:4444",
    ]
    assert proxy_candidates("http://127.0.0.1:4444") == [
        "http://127.0.0.1:4444",
        "socks5://127.0.0.1:4447",
    ]
    assert normalize_http_proxy("127.0.0.1:4447") == "socks5://127.0.0.1:4447"
    assert normalize_http_proxy("http://127.0.0.1:4447") == "socks5://127.0.0.1:4447"
    assert normalize_http_proxy("http://127.0.0.1:4444") == "http://127.0.0.1:4444"
    try:
        normalize_http_proxy("http://192.0.2.8:4447")
    except ValueError as exc:
        assert "localhost" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_extractors() -> None:
    assert extract_formtoken(HEAVY_LOAD) == "28a4409df2eb580494d2f2d3eae2a7672a1189b0"
    assert extract_download_id(SEARCH_HIT) == "42"


def test_download_via_postman_load_gate() -> None:
    opener = QueueOpener(
        [
            HEAVY_LOAD.encode(),
            b"<html>entered</html>",
            SEARCH_HIT.encode(),
            TORRENT_BYTES,
        ]
    )
    content, magnet = download_torrent_from_magnet(
        SAMPLE_MAGNET,
        opener_factory=lambda _proxy: opener,
        delay=0,
    )
    assert content == TORRENT_BYTES
    assert magnet.info_hash == "f6fee9fb500807cdbff9683e9136470c25717b5a"
    urls = [req.full_url for req in opener.requests]
    assert urls[0].endswith("search=f6fee9fb500807cdbff9683e9136470c25717b5a&category=-1")
    assert any("action=Enter" in url for url in urls)
    assert any("action=Download&id=42" in url for url in urls)
    assert opener.requests[1].data and b"formtoken=" in opener.requests[1].data


def test_retries_http_408() -> None:
    opener = QueueOpener(
        [
            b"HTTP_408",
            SEARCH_HIT.encode(),
            TORRENT_BYTES,
        ]
    )
    content, magnet = download_torrent_from_magnet(
        SAMPLE_MAGNET,
        opener_factory=lambda _proxy: opener,
        delay=0,
    )
    assert content == TORRENT_BYTES
    assert magnet.info_hash == "f6fee9fb500807cdbff9683e9136470c25717b5a"


def test_retries_http_503_ratelimit() -> None:
    opener = QueueOpener(
        [
            b"HTTP_503",
            SEARCH_HIT.encode(),
            TORRENT_BYTES,
        ]
    )
    content, magnet = download_torrent_from_magnet(
        SAMPLE_MAGNET,
        opener_factory=lambda _proxy: opener,
        delay=0,
    )
    assert content == TORRENT_BYTES
    assert magnet.info_hash == "f6fee9fb500807cdbff9683e9136470c25717b5a"


def test_retry_after_and_transport_errors() -> None:
    assert retry_after_seconds(b"Please wait at least 60 seconds and retry", delay=1) == 65.0
    assert retry_after_seconds(b"Please wait at least 60 seconds and retry", delay=0) == 0.0
    assert is_proxy_transport_error(MagnetError("Нет доступа к Postman через http://x: [Errno 61]"))
    assert not is_proxy_transport_error(MagnetError("Postman ограничил частоту запросов (ratelimit 60 с)."))


def test_socks_opener_ignores_system_http_proxy(monkeypatch: object) -> None:
    monkeypatch.setenv("http_proxy", "http://127.0.0.1:2080")
    monkeypatch.setenv("https_proxy", "http://127.0.0.1:2080")
    monkeypatch.setenv("ALL_PROXY", "http://127.0.0.1:2080")
    from i2ptorrents.magnet import _Socks5HTTPHandler, _build_opener

    opener = _build_opener("socks5://127.0.0.1:4447")
    assert any(isinstance(h, _Socks5HTTPHandler) for h in opener.handlers)
    for handler in opener.handlers:
        proxies = getattr(handler, "proxies", None) or {}
        assert "2080" not in str(proxies)
