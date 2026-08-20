from __future__ import annotations

import json
import os
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


def config_dir() -> Path:
    if sys.platform == "win32":
        root = Path(os.environ.get("APPDATA", Path.home()))
    elif sys.platform == "darwin":
        root = Path.home() / "Library" / "Application Support"
    else:
        root = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return root / "i2ptorrents-gui"


@dataclass(slots=True)
class AppSettings:
    rpc_url: str = "http://127.0.0.1:9191/mytorrents"
    torrents_dir: str = str(Path.home() / "torrents")
    refresh_seconds: int = 5
    theme: str = "light"

    @classmethod
    def load(cls, path: Path | None = None) -> "AppSettings":
        target = path or config_dir() / "settings.json"
        defaults = cls()
        try:
            data = json.loads(target.read_text(encoding="utf-8"))
            if not isinstance(data, dict):
                return defaults
            return cls(
                rpc_url=str(data.get("rpc_url", defaults.rpc_url)),
                torrents_dir=str(data.get("torrents_dir", str(Path.home() / "torrents"))),
                refresh_seconds=min(60, max(2, int(data.get("refresh_seconds", 5)))),
                theme="night" if data.get("theme") == "night" else "light",
            )
        except (OSError, ValueError, TypeError, json.JSONDecodeError):
            return defaults

    def save(self, path: Path | None = None) -> None:
        target = path or config_dir() / "settings.json"
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_suffix(".tmp")
        temporary.write_text(json.dumps(asdict(self), ensure_ascii=False, indent=2), encoding="utf-8")
        temporary.replace(target)
