from __future__ import annotations

import base64
from dataclasses import dataclass
from enum import IntEnum
from typing import Any, Mapping
from urllib.parse import quote

from .i18n import t


class TorrentStatus(IntEnum):
    STOPPED = 0
    QUEUED_VERIFY = 1
    VERIFYING = 2
    QUEUED_DOWNLOAD = 3
    DOWNLOADING = 4
    QUEUED_SEED = 5
    SEEDING = 6

    @property
    def label(self) -> str:
        return {
            self.STOPPED: t("status_stopped"),
            self.QUEUED_VERIFY: t("status_queued_verify"),
            self.VERIFYING: t("status_verifying"),
            self.QUEUED_DOWNLOAD: t("status_queued_download"),
            self.DOWNLOADING: t("status_downloading"),
            self.QUEUED_SEED: t("status_queued_seed"),
            self.SEEDING: t("status_seeding"),
        }[self]


def _value(data: Mapping[str, Any], camel: str, snake: str, default: Any = 0) -> Any:
    return data.get(camel, data.get(snake, default))


@dataclass(frozen=True, slots=True)
class Torrent:
    id: int
    name: str
    status: TorrentStatus
    total_size: int
    left_until_done: int
    rate_download: int
    rate_upload: int
    peers_sending_to_us: int
    peers_getting_from_us: int
    piece_count: int
    piece_size: int
    hash_string: str
    finished: bool
    pieces: tuple[bool, ...]

    @property
    def completed(self) -> int:
        return max(0, self.total_size - self.left_until_done)

    @property
    def progress(self) -> float:
        if self.total_size <= 0:
            return 1.0 if self.finished else 0.0
        return min(1.0, max(0.0, self.completed / self.total_size))

    @property
    def short_hash(self) -> str:
        digest = self.hash_string.strip()
        if len(digest) <= 16:
            return digest
        return f"{digest[:10]}…{digest[-6:]}"

    def magnet_uri(self, tracker: str = "http://tracker2.postman.i2p/announce.php") -> str:
        digest = self.hash_string.strip().lower()
        if not digest:
            return ""
        parts = [f"magnet:?xt=urn:btih:{digest}"]
        if self.name:
            parts.append(f"dn={quote(self.name)}")
        if tracker:
            parts.append(f"tr={quote(tracker, safe=':/')}")
        return "&".join(parts)

    @classmethod
    def from_rpc(cls, data: Mapping[str, Any]) -> "Torrent":
        raw_status = int(data.get("status", 0))
        try:
            status = TorrentStatus(raw_status)
        except ValueError:
            status = TorrentStatus.STOPPED
        total = int(_value(data, "totalSize", "total_size", _value(data, "sizeWhenDone", "size_when_done")))
        piece_count = max(0, int(_value(data, "pieceCount", "piece_count")))
        finished = bool(_value(data, "isFinished", "is_finished", False))
        return cls(
            id=int(data["id"]),
            name=str(data.get("name") or t("untitled")),
            status=status,
            total_size=max(0, total),
            left_until_done=max(0, int(_value(data, "leftUntilDone", "left_until_done"))),
            rate_download=max(0, int(_value(data, "rateDownload", "rate_download"))),
            rate_upload=max(0, int(_value(data, "rateUpload", "rate_upload"))),
            peers_sending_to_us=max(0, int(_value(data, "peersSendingToUs", "peers_sending_to_us"))),
            peers_getting_from_us=max(0, int(_value(data, "peersGettingFromUs", "peers_getting_from_us"))),
            piece_count=piece_count,
            piece_size=max(0, int(_value(data, "pieceSize", "piece_size"))),
            hash_string=str(_value(data, "hashString", "hash_string", "")).strip(),
            finished=finished,
            pieces=decode_piece_bitfield(_value(data, "pieces", "pieces", ""), piece_count, finished),
        )


def decode_piece_bitfield(raw: Any, piece_count: int, finished: bool = False) -> tuple[bool, ...]:
    count = max(0, piece_count)
    if count <= 0:
        return ()
    if isinstance(raw, (bytes, bytearray)):
        data = bytes(raw)
    else:
        text = str(raw or "").strip()
        if not text:
            return (True,) * count if finished else ()
        try:
            data = base64.b64decode(text)
        except (ValueError, TypeError):
            return ()
    flags = []
    for index in range(count):
        offset = index // 8
        bit = 0x80 >> (index % 8)
        flags.append(bool(offset < len(data) and data[offset] & bit))
    return tuple(flags)


def format_bytes(value: int) -> str:
    size = float(max(0, value))
    units = ("unit_b", "unit_kb", "unit_mb", "unit_gb", "unit_tb")
    for key in units:
        if size < 1024 or key == units[-1]:
            label = t(key)
            return f"{size:.0f} {label}" if key == "unit_b" else f"{size:.1f} {label}"
        size /= 1024
    return f"{size:.1f} {t('unit_tb')}"


def format_rate(value: int) -> str:
    return f"{format_bytes(value)}{t('per_second')}"
