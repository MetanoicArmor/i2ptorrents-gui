from pathlib import Path
import shutil
import subprocess
import sys

from PIL import Image


def square_crop(img: Image.Image) -> Image.Image:
    width, height = img.size
    side = min(width, height)
    left = (width - side) // 2
    top = (height - side) // 2
    return img.crop((left, top, left + side, top + side))


def zero_rgb_under_transparent(img: Image.Image) -> Image.Image:
    """White RGB under A=0 becomes a grey macOS plate/halo around the icon."""
    rgba = img.convert("RGBA")
    alpha = rgba.getchannel("A")
    rgb = Image.new("RGB", rgba.size, (0, 0, 0))
    rgb.paste(rgba.convert("RGB"), mask=alpha)
    out = rgb.convert("RGBA")
    out.putalpha(alpha)
    return out


def opaque_full_bleed(img: Image.Image, threshold: int = 250) -> Image.Image:
    """Crop to a fully opaque square so macOS can apply its own squircle mask."""
    rgba = img.convert("RGBA")
    bbox = rgba.getchannel("A").getbbox()
    if bbox is None:
        return square_crop(rgba)
    cropped = square_crop(rgba.crop(bbox))
    width, height = cropped.size
    alpha = cropped.getchannel("A")

    def corners_opaque(inset: int) -> bool:
        if inset < 0 or inset * 2 >= min(width, height):
            return False
        samples = (
            (inset, inset),
            (width - 1 - inset, inset),
            (inset, height - 1 - inset),
            (width - 1 - inset, height - 1 - inset),
        )
        return all(alpha.getpixel(point) >= threshold for point in samples)

    low, high = 0, min(width, height) // 2
    inset = 0
    found = False
    while low <= high:
        mid = (low + high) // 2
        if corners_opaque(mid):
            inset = mid
            found = True
            high = mid - 1
        else:
            low = mid + 1
    if not found:
        return cropped
    return square_crop(cropped.crop((inset, inset, width - inset, height - inset)))


def _downscale(master: Image.Image, pixels: int) -> Image.Image:
    return master.resize((pixels, pixels), Image.Resampling.LANCZOS)


def make_icon() -> None:
    root = Path(__file__).parent
    src_name = sys.argv[1] if len(sys.argv) > 1 else "image.png"
    src = root / src_name
    out_icon = root / "icon.png"

    if not src.exists():
        raise SystemExit(f"source image not found: {src}")

    source = Image.open(src).convert("RGBA")
    rounded = zero_rgb_under_transparent(square_crop(source))
    macos = opaque_full_bleed(source)
    icon_1024 = _downscale(rounded, 1024)
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
        _downscale(macos, size).save(iconset / f"icon_{size}x{size}.png")
        _downscale(macos, size * 2).save(iconset / f"icon_{size}x{size}@2x.png")

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
