from __future__ import annotations

import base64
import http.client
import http.cookiejar
import ipaddress
import re
import socket
import time
from dataclasses import dataclass
from typing import Callable
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, quote, unquote_plus, urljoin, urlsplit, urlunsplit
from urllib.request import HTTPCookieProcessor, HTTPHandler, HTTPRedirectHandler, ProxyHandler, Request, build_opener

from .i18n import t
from .rpc import RPCError

DEFAULT_HTTP_PROXY = "http://127.0.0.1:4444"
DEFAULT_SOCKS_PROXY = "socks5://127.0.0.1:4447"
DEFAULT_I2P_PROXY = DEFAULT_SOCKS_PROXY
DEFAULT_TRACKER = "http://tracker2.postman.i2p"
MAX_TORRENT_BYTES = 8 * 1024 * 1024
REQUEST_DELAY_SECONDS = 6.0
_RETRY_AFTER_RE = re.compile(r"wait at least (\d+) seconds", re.IGNORECASE)

_FORMTOKEN_RE = re.compile(
    r'name=["\']formtoken["\'][^>]*value=["\']([^"\']+)["\']|'
    r'value=["\']([^"\']+)["\'][^>]*name=["\']formtoken["\']',
    re.IGNORECASE,
)
_DOWNLOAD_RE = re.compile(r'index\.php\?action=Download&id=([0-9A-Za-z]+)', re.IGNORECASE)
_BTIH_HEX_RE = re.compile(r"^[0-9a-fA-F]{40}$")
_BTIH_B32_RE = re.compile(r"^[A-Z2-7]{32}$")


class MagnetError(RPCError):
    """Magnet parse or Postman fetch error."""


@dataclass(frozen=True, slots=True)
class MagnetLink:
    info_hash: str
    name: str
    trackers: tuple[str, ...]
    uri: str

    @property
    def filename(self) -> str:
        base = self.name or self.info_hash
        cleaned = re.sub(r"[^\w.\- ()\[\]]+", "_", base, flags=re.UNICODE).strip("._ ")
        return f"{cleaned[:120] or self.info_hash}.torrent"


def parse_magnet(value: str) -> MagnetLink:
    raw = value.strip()
    if not raw.lower().startswith("magnet:"):
        raise MagnetError(t("not_magnet"))
    parsed = urlsplit(raw)
    query = parse_qs(parsed.query, keep_blank_values=False)
    info_hash = ""
    for xt in query.get("xt", []):
        item = unquote_plus(xt).strip()
        prefix = "urn:btih:"
        if item.lower().startswith(prefix):
            info_hash = _normalize_info_hash(item[len(prefix) :])
            break
    if not info_hash:
        raise MagnetError(t("magnet_no_hash"))
    names = query.get("dn", [])
    name = unquote_plus(names[0]).strip() if names else ""
    trackers = tuple(unquote_plus(item).strip() for item in query.get("tr", []) if item.strip())
    return MagnetLink(info_hash=info_hash, name=name, trackers=trackers, uri=raw)


def tracker_web_url(magnet: MagnetLink, fallback: str = DEFAULT_TRACKER) -> str:
    for tracker in magnet.trackers:
        parsed = urlsplit(tracker if "://" in tracker else "http://" + tracker)
        host = (parsed.hostname or "").lower()
        if host.endswith(".i2p"):
            return urlunsplit((parsed.scheme or "http", parsed.netloc, "", "", "")).rstrip("/")
    return fallback.rstrip("/")


def normalize_http_proxy(value: str) -> str:
    raw = value.strip() or DEFAULT_I2P_PROXY
    if "://" not in raw:
        raw = f"socks5://{raw}" if raw.rsplit(":", 1)[-1] != "4444" else f"http://{raw}"
    parsed = urlsplit(raw)
    if parsed.scheme not in {"http", "socks", "socks5"} or not parsed.hostname:
        raise ValueError(t("proxy_invalid"))
    host = parsed.hostname.rstrip(".").lower()
    if host != "localhost":
        try:
            if not ipaddress.ip_address(host).is_loopback:
                raise ValueError
        except ValueError as exc:
            raise ValueError(t("proxy_local_only")) from exc
    scheme = "socks5" if parsed.scheme in {"socks", "socks5"} else "http"
    port = parsed.port or (4447 if scheme == "socks5" else 4444)
    if port == 4447:
        scheme = "socks5"
    hostname = parsed.hostname
    if ":" in hostname and not hostname.startswith("["):
        hostname = f"[{hostname}]"
    return f"{scheme}://{hostname}:{port}"


def proxy_candidates(preferred: str) -> list[str]:
    """SOCKS 4447 first: HTTP-прокси i2pd подменяет Postman на .b32.i2p и ломает cookies."""
    pref = normalize_http_proxy(preferred)
    ordered = [DEFAULT_SOCKS_PROXY, DEFAULT_HTTP_PROXY]
    if pref in ordered:
        ordered.remove(pref)
        ordered.insert(0, pref)
    else:
        ordered.insert(0, pref)
    return ordered


def retry_after_seconds(body: bytes | str, delay: float, default: float = 65.0) -> float:
    if delay <= 0:
        return 0.0
    text = body.decode("utf-8", "replace") if isinstance(body, bytes) else body
    match = _RETRY_AFTER_RE.search(text)
    if match:
        return min(180.0, float(match.group(1)) + 5.0)
    return default


def is_proxy_transport_error(exc: BaseException) -> bool:
    text = str(exc).lower()
    if "ratelimit" in text or "http 503" in text or "не найден" in text or "не отдал" in text:
        return False
    if "not found" in text or "did not return" in text:
        return False
    if "не ответил вовремя" in text or "did not answer in time" in text or "http 408" in text:
        return False
    markers = (
        "connection refused",
        "errno 61",
        "errno 111",
        "socks5-прокси i2pd отклонил",
        "socks5 proxy rejected",
        "socks5 connect",
        "прокси i2pd закрыл",
        "socks5 proxy closed",
        "нет доступа к postman",
        "no access to postman",
    )
    return any(marker in text for marker in markers)


def download_torrent_from_magnet(
    value: str,
    http_proxy: str = DEFAULT_I2P_PROXY,
    tracker_url: str = DEFAULT_TRACKER,
    opener_factory: Callable[[str], object] | None = None,
    delay: float = REQUEST_DELAY_SECONDS,
) -> tuple[bytes, MagnetLink]:
    magnet = parse_magnet(value)
    base = tracker_web_url(magnet, tracker_url)
    if opener_factory is not None:
        opener = opener_factory(normalize_http_proxy(http_proxy))
        content = _PostmanClient(opener, base, delay=delay).fetch_torrent(magnet)
        return content, magnet
    errors: list[str] = []
    for proxy in proxy_candidates(http_proxy):
        try:
            opener = _build_opener(proxy)
            content = _PostmanClient(opener, base, delay=delay).fetch_torrent(magnet)
            return content, magnet
        except MagnetError as exc:
            errors.append(f"{proxy}: {exc}")
            if not is_proxy_transport_error(exc):
                break
    raise MagnetError(t("magnet_failed", errors="\n".join(errors)))


def extract_formtoken(html: str) -> str | None:
    match = _FORMTOKEN_RE.search(html)
    if not match:
        return None
    return match.group(1) or match.group(2)


def extract_download_id(html: str) -> str | None:
    match = _DOWNLOAD_RE.search(html)
    return match.group(1) if match else None


def looks_like_torrent(content: bytes) -> bool:
    if not content.startswith(b"d") or len(content) < 16:
        return False
    return b"4:info" in content or b"8:announce" in content or b"13:announce-list" in content


def is_rate_limited(status: int, content: bytes) -> bool:
    if status in {429, 503}:
        return True
    lowered = content.lower()
    return b"ratelimit active" in lowered or b"throttling" in lowered


def _normalize_info_hash(value: str) -> str:
    raw = value.strip().replace(" ", "")
    if _BTIH_HEX_RE.fullmatch(raw):
        return raw.lower()
    compact = raw.upper().replace("=", "")
    if _BTIH_B32_RE.fullmatch(compact):
        decoded = base64.b32decode(compact + "=" * ((8 - len(compact) % 8) % 8))
        if len(decoded) != 20:
            raise MagnetError(t("bad_base32_hash"))
        return decoded.hex()
    raise MagnetError(t("bad_info_hash"))


def _build_opener(proxy: str):
    parsed = urlsplit(proxy)
    handlers: list[object] = [_I2PRedirects(), HTTPCookieProcessor(http.cookiejar.CookieJar())]
    if parsed.scheme in {"socks", "socks5"}:
        # Пустой ProxyHandler отключает системный HTTP_PROXY (у вас это 127.0.0.1:2080).
        handlers.insert(0, ProxyHandler({}))
        handlers.insert(0, _Socks5HTTPHandler(parsed.hostname or "127.0.0.1", parsed.port or 4447))
    else:
        handlers.insert(0, ProxyHandler({"http": proxy, "https": proxy}))
    return build_opener(*handlers)


def _recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        piece = sock.recv(size - len(chunks))
        if not piece:
            raise MagnetError(t("socks_closed"))
        chunks.extend(piece)
    return bytes(chunks)


def _socks5_connect(proxy_host: str, proxy_port: int, dest_host: str, dest_port: int, timeout: float | None) -> socket.socket:
    host_name = dest_host.split("%")[0].rstrip(".")
    if host_name in {"localhost", "127.0.0.1", "::1"} or host_name.startswith("127."):
        raise MagnetError(t("socks_loopback", host=host_name, port=dest_port))
    sock = socket.create_connection((proxy_host, proxy_port), timeout)
    try:
        sock.sendall(b"\x05\x01\x00")
        hello = _recv_exact(sock, 2)
        if hello != b"\x05\x00":
            raise MagnetError(t("socks_rejected"))
        host = dest_host.encode("idna")
        if len(host) > 255:
            raise MagnetError(t("socks_host_long"))
        sock.sendall(b"\x05\x01\x00\x03" + bytes([len(host)]) + host + dest_port.to_bytes(2, "big"))
        header = _recv_exact(sock, 4)
        if header[0] != 5 or header[1] != 0:
            status = header[1] if len(header) > 1 else -1
            detail = t(f"socks5_{status}") if 1 <= status <= 8 else t("socks5_code", code=status)
            raise MagnetError(t("socks_failed", host=dest_host, port=dest_port, detail=detail))
        atyp = header[3]
        if atyp == 1:
            _recv_exact(sock, 6)
        elif atyp == 4:
            _recv_exact(sock, 18)
        elif atyp == 3:
            length = _recv_exact(sock, 1)[0]
            _recv_exact(sock, length + 2)
        else:
            raise MagnetError(t("socks_unknown"))
    except Exception:
        sock.close()
        raise
    return sock


class _Socks5HTTPConnection(http.client.HTTPConnection):
    def __init__(self, host: str, port: int | None = None, timeout: float = 45, proxy_host: str = "127.0.0.1", proxy_port: int = 4447, **kwargs: object) -> None:
        super().__init__(host, port, timeout=timeout, **kwargs)  # type: ignore[arg-type]
        self._proxy_host = proxy_host
        self._proxy_port = proxy_port
        self._http_vsn = 10
        self._http_vsn_str = "HTTP/1.0"

    def connect(self) -> None:
        self.sock = _socks5_connect(
            self._proxy_host,
            self._proxy_port,
            self.host,
            self.port or 80,
            self.timeout,
        )


class _Socks5HTTPHandler(HTTPHandler):
    def __init__(self, proxy_host: str, proxy_port: int) -> None:
        super().__init__()
        self.proxy_host = proxy_host
        self.proxy_port = proxy_port

    def http_open(self, req: Request):  # type: ignore[no-untyped-def]
        proxy_host = self.proxy_host
        proxy_port = self.proxy_port

        def connection(*args: object, **kwargs: object) -> _Socks5HTTPConnection:
            return _Socks5HTTPConnection(*args, proxy_host=proxy_host, proxy_port=proxy_port, **kwargs)  # type: ignore[misc]

        return self.do_open(connection, req)


class _I2PRedirects(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        resolved = urljoin(req.full_url, newurl)
        old_host = (urlsplit(req.full_url).hostname or "").lower()
        host = (urlsplit(resolved).hostname or "").lower()
        if host and host != old_host and not host.endswith(".i2p"):
            return None
        if host and not (host.endswith(".i2p") or host in {"localhost", "127.0.0.1", "::1"}):
            raise MagnetError(t("redirect_outside_i2p"))
        return super().redirect_request(req, fp, code, msg, headers, resolved)


class _PostmanClient:
    def __init__(self, opener: object, base: str, delay: float) -> None:
        self._opener = opener
        self._base = base.rstrip("/")
        self._delay = max(0.0, delay)
        self._last_request = 0.0

    def fetch_torrent(self, magnet: MagnetLink) -> bytes:
        search_url = (
            f"{self._base}/index.php?view=Main&search={quote(magnet.info_hash)}&category=-1"
        )
        html = self._pass_load_gate(self._open(search_url), search_url)
        torrent_id = extract_download_id(html)
        if torrent_id is None and magnet.name:
            named_url = (
                f"{self._base}/index.php?view=Main&search={quote(magnet.name)}&category=-1"
            )
            html = self._pass_load_gate(self._open(named_url), named_url)
            torrent_id = extract_download_id(html)
        if torrent_id is None:
            raise MagnetError(t("postman_not_found"))
        content = self._open_bytes(
            f"{self._base}/index.php?action=Download&id={quote(torrent_id, safe='')}"
        )
        if not looks_like_torrent(content):
            raise MagnetError(t("postman_not_torrent"))
        return content

    def _pass_load_gate(self, html: str, resume_url: str) -> str:
        if "UNDER HEAVY LOAD" not in html.upper():
            return html
        token = extract_formtoken(html)
        if not token:
            raise MagnetError(t("postman_no_token"))
        try:
            self._open(self._base + "/?action=Enter", {"formtoken": token})
        except MagnetError:
            pass
        return self._open(resume_url)

    def _open(self, url: str, data: dict[str, str] | None = None) -> str:
        return self._open_bytes(url, data).decode("utf-8", "replace")

    def _open_bytes(self, url: str, data: dict[str, str] | None = None) -> bytes:
        from urllib.parse import urlencode

        self._throttle()
        encoded = urlencode(data).encode() if data is not None else None
        headers = {
            "User-Agent": "i2ptorrents-gui/0.1",
            "Accept": "*/*",
            "Connection": "close",
        }
        if encoded is not None:
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        last_error: Exception | None = None
        attempts = 4
        for attempt in range(attempts):
            request = Request(url, data=encoded, headers=headers)
            try:
                with self._opener.open(request, timeout=45) as response:  # type: ignore[attr-defined]
                    raw = response.read(MAX_TORRENT_BYTES + 1)
                    status = getattr(response, "status", 0) or 0
            except HTTPError as exc:
                raw = exc.read(MAX_TORRENT_BYTES + 1)
                if looks_like_torrent(raw):
                    if len(raw) > MAX_TORRENT_BYTES:
                        raise MagnetError(t("postman_too_large")) from exc
                    return raw
                if exc.code in {301, 302, 303, 307, 308}:
                    raise MagnetError(
                        t(
                            "postman_redirect",
                            location=exc.headers.get("Location") if exc.headers else t("no_location"),
                        )
                    ) from exc
                if is_rate_limited(exc.code, raw):
                    if attempt == 0:
                        time.sleep(retry_after_seconds(raw, self._delay))
                        last_error = exc
                        continue
                    raise MagnetError(t("postman_ratelimit")) from exc
                if exc.code in {408, 429, 502, 504} and attempt < attempts - 1:
                    time.sleep(0.0 if self._delay <= 0 else (2.0 if exc.code == 408 else self._delay))
                    last_error = exc
                    continue
                if exc.code == 408:
                    raise MagnetError(t("postman_408")) from exc
                raise MagnetError(t("postman_http", code=exc.code, reason=exc.reason)) from exc
            except (URLError, TimeoutError, OSError) as exc:
                last_error = exc
                if attempt < attempts - 1:
                    time.sleep(self._delay * (attempt + 1) if self._delay else 0)
                    continue
                reason = getattr(exc, "reason", exc)
                raise MagnetError(t("postman_no_access", base=self._base, reason=reason)) from exc
            if len(raw) > MAX_TORRENT_BYTES:
                raise MagnetError(t("postman_too_large"))
            if is_rate_limited(status, raw):
                if attempt == 0:
                    time.sleep(retry_after_seconds(raw, self._delay))
                    continue
                raise MagnetError(t("postman_ratelimit"))
            return raw
        raise MagnetError(t("postman_failed", error=last_error))

    def _throttle(self) -> None:
        if self._delay <= 0:
            self._last_request = time.monotonic()
            return
        wait = self._last_request + self._delay - time.monotonic()
        if wait > 0:
            time.sleep(wait)
        self._last_request = time.monotonic()
