from PIL import Image

from make_icon import opaque_full_bleed, square_crop, zero_rgb_under_transparent


def _rounded_square(side: int = 64, radius: int = 12) -> Image.Image:
    img = Image.new("RGBA", (side, side), (255, 255, 255, 0))
    for y in range(side):
        for x in range(side):
            dx = min(x, side - 1 - x)
            dy = min(y, side - 1 - y)
            if dx >= radius or dy >= radius or (radius - dx) ** 2 + (radius - dy) ** 2 <= radius * radius:
                img.putpixel((x, y), (40, 80, 180, 255))
    return img


def test_zero_rgb_under_transparent_clears_white_under_alpha() -> None:
    img = Image.new("RGBA", (4, 4), (255, 255, 255, 0))
    img.putpixel((1, 1), (10, 20, 30, 255))
    cleaned = zero_rgb_under_transparent(img)
    assert cleaned.getpixel((0, 0))[:3] == (0, 0, 0)
    assert cleaned.getpixel((0, 0))[3] == 0
    assert cleaned.getpixel((1, 1)) == (10, 20, 30, 255)


def test_opaque_full_bleed_has_no_transparent_corners() -> None:
    bleed = opaque_full_bleed(_rounded_square())
    width, height = bleed.size
    assert width == height
    for point in ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1)):
        assert bleed.getpixel(point)[3] == 255


def test_square_crop_centers_on_shortest_side() -> None:
    img = Image.new("RGBA", (10, 6), (1, 2, 3, 255))
    cropped = square_crop(img)
    assert cropped.size == (6, 6)
