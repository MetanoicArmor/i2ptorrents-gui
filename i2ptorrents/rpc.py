from __future__ import annotations

import base64
import ipaddress
import json
import socket
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit, urlunsplit
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener

from .models import Torrent


class RPCError(RuntimeError):
    """A user-displayable RPC transport or protocol error."""


def normalize_rpc_url(value: str) -> str:
    raw = value.strip()
    if not raw:
        raise ValueError("Укажите адрес RPC")
    if "://" not in raw:
        raw = "http://" + raw
    parsed = urlsplit(raw)
    if parsed.scheme not in {"http", "https"} or not parsed.hostname:
        raise ValueError("Некорректный адрес RPC")
    host = parsed.hostname.rstrip(".").lower()
    if host != "localhost":
        try:
            if not ipaddress.ip_address(host).is_loopback:
                raise ValueError
        except ValueError as exc:
            raise ValueError(
                "RPC i2pd не имеет авторизации: разрешён только локальный адрес"
            ) from exc
    path = parsed.path.rstrip("/")
    if not path.endswith("/rpc"):
        path += "/rpc"
    path += "/"
    return urlunsplit((parsed.scheme, parsed.netloc, path, "", ""))


class TransmissionRPC:
    MAX_RESPONSE_BYTES = 8 * 1024 * 1024
    FIELDS = (
        "id", "name", "status", "isFinished", "sizeWhenDone", "leftUntilDone",
        "rateDownload", "rateUpload", "peersGettingFromUs", "peersSendingToUs",
        "pieceCount", "pieceSize", "totalSize", "hashString",
    )

    def __init__(self, endpoint: str, timeout: float = 5.0) -> None:
        self.endpoint = normalize_rpc_url(endpoint)
        self.timeout = timeout
        self._tag = 0
        self._opener = build_opener(ProxyHandler({}), _RejectRedirects())

    def _call(self, method: str, arguments: dict[str, Any]) -> dict[str, Any]:
        self._tag += 1
        body = json.dumps(
            {"method": method, "arguments": arguments, "tag": self._tag},
            separators=(",", ":"),
        ).encode()
        request = Request(
            self.endpoint,
            data=body,
            headers={
                "Content-Type": "application/json",
                "Accept": "application/json",
                "User-Agent": "i2ptorrents-gui/0.1",
            },
            method="POST",
        )
        try:
            with self._opener.open(request, timeout=self.timeout) as response:
                raw = response.read(self.MAX_RESPONSE_BYTES + 1)
                if len(raw) > self.MAX_RESPONSE_BYTES:
                    raise RPCError("Ответ i2pd слишком велик")
                payload = json.loads(raw.decode("utf-8"))
        except HTTPError as exc:
            detail = exc.read().decode("utf-8", "replace").strip()
            raise RPCError(f"RPC вернул HTTP {exc.code}: {detail or exc.reason}") from exc
        except (URLError, socket.timeout, TimeoutError, OSError) as exc:
            reason = getattr(exc, "reason", exc)
            raise RPCError(f"Нет соединения с i2pd: {reason}") from exc
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise RPCError("i2pd вернул некорректный ответ") from exc

        if not isinstance(payload, dict):
            raise RPCError("Некорректный формат ответа i2pd")
        if "error" in payload:
            error = payload["error"]
            message = error.get("message", "неизвестная ошибка") if isinstance(error, dict) else str(error)
            raise RPCError(f"Ошибка RPC: {message}")
        if payload.get("result") not in (None, "success") and not isinstance(payload.get("result"), dict):
            raise RPCError(f"Ошибка RPC: {payload['result']}")
        result = payload.get("arguments", payload.get("result", {}))
        return result if isinstance(result, dict) else {}

    def get_torrents(self, ids: list[int] | None = None) -> list[Torrent]:
        arguments: dict[str, Any] = {"fields": list(self.FIELDS)}
        if ids is not None:
            arguments["ids"] = ids
        result = self._call("torrent-get", arguments)
        rows = result.get("torrents", [])
        if not isinstance(rows, list):
            raise RPCError("Некорректный список торрентов")
        return [Torrent.from_rpc(row) for row in rows if isinstance(row, dict)]

    def add_torrent(self, path: Path) -> dict[str, Any]:
        try:
            content = path.read_bytes()
        except OSError as exc:
            raise RPCError(f"Не удалось прочитать файл: {exc}") from exc
        if not content:
            raise RPCError("Torrent-файл пуст")
        result = self._call("torrent-add", {"metainfo": base64.b64encode(content).decode("ascii")})
        added = result.get("torrent-added") or result.get("torrent-duplicate")
        return added if isinstance(added, dict) else {}

    def remove_torrent(self, torrent_id: int, delete_data: bool = False) -> None:
        self._call("torrent-remove", {"ids": [torrent_id], "delete-local-data": delete_data})


class _RejectRedirects(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        raise RPCError("i2pd RPC не должен перенаправлять запросы")
