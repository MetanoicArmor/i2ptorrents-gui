from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Any, Mapping


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
            self.STOPPED: "Остановлен",
            self.QUEUED_VERIFY: "В очереди на проверку",
            self.VERIFYING: "Проверка",
            self.QUEUED_DOWNLOAD: "В очереди",
            self.DOWNLOADING: "Загрузка",
            self.QUEUED_SEED: "В очереди на раздачу",
            self.SEEDING: "Раздаётся",
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

    @property
    def completed(self) -> int:
        return max(0, self.total_size - self.left_until_done)

    @property
    def progress(self) -> float:
        if self.total_size <= 0:
            return 1.0 if self.finished else 0.0
        return min(1.0, max(0.0, self.completed / self.total_size))

    @classmethod
    def from_rpc(cls, data: Mapping[str, Any]) -> "Torrent":
        raw_status = int(data.get("status", 0))
        try:
            status = TorrentStatus(raw_status)
        except ValueError:
            status = TorrentStatus.STOPPED
        total = int(_value(data, "totalSize", "total_size", _value(data, "sizeWhenDone", "size_when_done")))
        return cls(
            id=int(data["id"]),
            name=str(data.get("name", "Без имени")),
            status=status,
            total_size=max(0, total),
            left_until_done=max(0, int(_value(data, "leftUntilDone", "left_until_done"))),
            rate_download=max(0, int(_value(data, "rateDownload", "rate_download"))),
            rate_upload=max(0, int(_value(data, "rateUpload", "rate_upload"))),
            peers_sending_to_us=max(0, int(_value(data, "peersSendingToUs", "peers_sending_to_us"))),
            peers_getting_from_us=max(0, int(_value(data, "peersGettingFromUs", "peers_getting_from_us"))),
            piece_count=max(0, int(_value(data, "pieceCount", "piece_count"))),
            piece_size=max(0, int(_value(data, "pieceSize", "piece_size"))),
            hash_string=str(_value(data, "hashString", "hash_string", "")),
            finished=bool(_value(data, "isFinished", "is_finished", False)),
        )


def format_bytes(value: int) -> str:
    size = float(max(0, value))
    units = ("Б", "КБ", "МБ", "ГБ", "ТБ")
    for unit in units:
        if size < 1024 or unit == units[-1]:
            return f"{size:.0f} {unit}" if unit == "Б" else f"{size:.1f} {unit}"
        size /= 1024
    return f"{size:.1f} ТБ"


def format_rate(value: int) -> str:
    return f"{format_bytes(value)}/с"
