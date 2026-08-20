from pathlib import Path
import sys

from i2ptorrents import application_icon_path


def test_source_icon_falls_back_to_image_png() -> None:
    if getattr(sys, "frozen", False):
        return
    path = application_icon_path()
    assert path is not None
    assert path.name in {"icon.png", "I2PTorrents.ico", "image.png"}
    assert path.is_file()
    assert path.parent == Path(__file__).resolve().parent.parent
