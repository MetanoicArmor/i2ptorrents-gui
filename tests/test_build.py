from pathlib import Path

import pytest

from i2ptorrents.build import build_command, repo_root


def test_repo_root_contains_packaging_scripts() -> None:
    root = repo_root()
    assert (root / "build-macos.sh").is_file()
    assert (root / "build-linux.sh").is_file()
    assert (root / "build-windows.ps1").is_file()


def test_build_command_selects_platform_script() -> None:
    root = Path("/tmp/i2ptorrents")
    assert build_command(root, "darwin") == ["bash", str(root / "build-macos.sh")]
    assert build_command(root, "linux") == ["bash", str(root / "build-linux.sh")]
    windows = build_command(root, "win32")
    assert windows[-1] == str(root / "build-windows.ps1")
    assert windows[0] == "powershell"


def test_build_command_rejects_unknown_platform() -> None:
    with pytest.raises(SystemExit):
        build_command(Path("."), "plan9")
