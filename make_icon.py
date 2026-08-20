from pathlib import Path
import shutil
import subprocess
import sys

from PIL import Image


def make_icon() -> None:
    root = Path(__file__).parent
    src_name = sys.argv[1] if len(sys.argv) > 1 else "image.png"
    src = root / src_name
    out_icon = root / "icon.png"

    if not src.exists():
        raise SystemExit(f"source image not found: {src}")

    img = Image.open(src).convert("RGBA")
    width, height = img.size
    side = min(width, height)
    left = (width - side) // 2
    top = (height - side) // 2
    master = img.crop((left, top, left + side, top + side))
    resample = Image.Resampling.LANCZOS

    def downscale(pixels: int) -> Image.Image:
        return master.resize((pixels, pixels), resample)

    icon_1024 = downscale(1024)
    icon_1024.save(out_icon)
    print("saved", out_icon)

    out_ico = root / "I2PTorrents.ico"
    icon_1024.save(
        out_ico,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print("saved", out_ico)

    out_icns = root / "I2PTorrents.icns"
    iconset = root / "I2PTorrents.iconset"
    iconset.mkdir(exist_ok=True)
    for size in (16, 32, 128, 256, 512):
        downscale(size).save(iconset / f"icon_{size}x{size}.png")
        downscale(size * 2).save(iconset / f"icon_{size}x{size}@2x.png")

    iconutil = shutil.which("iconutil")
    if iconutil:
        subprocess.run(
            [iconutil, "-c", "icns", str(iconset), "-o", str(out_icns)],
            check=True,
        )
        print("saved", out_icns)
    else:
        print("skip icns: iconutil not found")

    for path in iconset.glob("*.png"):
        path.unlink(missing_ok=True)
    iconset.rmdir()


if __name__ == "__main__":
    try:
        make_icon()
    except Exception as exc:
        raise SystemExit(f"make_icon failed: {exc}") from exc
