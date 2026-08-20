"""Desktop client for the i2pd torrents tunnel."""

from __future__ import annotations

import sys
from pathlib import Path

APP_NAME = "I2P Torrents"
APP_AUTHOR = "Vade"
APP_LICENSE = "BSD-3-Clause"


def resource_roots() -> list[Path]:
    roots: list[Path] = []
    if getattr(sys, "frozen", False):
        meipass = getattr(sys, "_MEIPASS", None)
        if meipass:
            roots.append(Path(meipass))
        exe_dir = Path(sys.executable).resolve().parent
        roots.extend((exe_dir, exe_dir.parent / "Resources"))
    roots.append(Path(__file__).resolve().parent.parent)
    seen: set[Path] = set()
    unique: list[Path] = []
    for root in roots:
        try:
            resolved = root.resolve()
        except OSError:
            continue
        if resolved in seen:
            continue
        seen.add(resolved)
        unique.append(resolved)
    return unique


def resource_path(name: str) -> Path | None:
    for root in resource_roots():
        path = root / name
        if path.is_file():
            return path
    return None


def application_icon_path() -> Path | None:
    for name in ("icon.png", "I2PTorrents.ico", "image.png"):
        path = resource_path(name)
        if path is not None:
            return path
    return None


def read_version(default: str = "0.1.0") -> str:
    path = resource_path("VERSION")
    if path is None:
        return default
    text = path.read_text(encoding="utf-8").strip()
    return text or default


__version__ = read_version()
