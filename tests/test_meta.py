from pathlib import Path

from i2ptorrents import APP_AUTHOR, APP_LICENSE, __version__, read_version, resource_path


def test_version_file_is_010() -> None:
    path = resource_path("VERSION")
    assert path is not None
    assert path.read_text(encoding="utf-8").strip() == "0.1.0"
    assert read_version() == "0.1.0"
    assert __version__ == "0.1.0"


def test_authors_and_bsd_license() -> None:
    authors = resource_path("AUTHORS")
    license_path = resource_path("LICENSE")
    assert authors is not None
    assert license_path is not None
    assert "Vade" in authors.read_text(encoding="utf-8")
    text = license_path.read_text(encoding="utf-8")
    assert "Redistribution and use in source and binary forms" in text
    assert "Neither the name of the copyright holder" in text
    assert APP_AUTHOR == "Vade"
    assert APP_LICENSE == "BSD-3-Clause"


def test_resource_paths_live_in_repo_root() -> None:
    root = Path(__file__).resolve().parent.parent
    for name in ("VERSION", "AUTHORS", "LICENSE"):
        path = resource_path(name)
        assert path == root / name
