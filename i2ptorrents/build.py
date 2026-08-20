"""Launch the platform packaging script from the repo root."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def build_command(root: Path, platform: str) -> list[str]:
    if platform == "darwin":
        return ["bash", str(root / "build-macos.sh")]
    if platform.startswith("linux"):
        return ["bash", str(root / "build-linux.sh")]
    if platform == "win32":
        return [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(root / "build-windows.ps1"),
        ]
    raise SystemExit(f"unsupported platform: {platform}")


def main() -> int:
    root = repo_root()
    command = build_command(root, sys.platform)
    script = Path(command[-1])
    if not script.is_file():
        print(f"ERROR: build script not found: {script}", file=sys.stderr)
        return 1
    return int(subprocess.run(command, cwd=root).returncode)


if __name__ == "__main__":
    raise SystemExit(main())
