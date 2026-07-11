from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "app" / "assets"
PNG_PATH = ASSET_DIR / "app_icon_256.png"
ICO_PATH = ASSET_DIR / "app_icon.ico"


def lerp(a: int, b: int, t: float) -> int:
    return round(a + (b - a) * t)


def mix(c1: tuple[int, int, int], c2: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(lerp(a, b, t) for a, b in zip(c1, c2))


def rounded_gradient(size: int, radius: int) -> Image.Image:
    top = (61, 139, 143)
    bottom = (35, 79, 84)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pixels = img.load()
    for y in range(size):
        for x in range(size):
            t = (x * 0.25 + y * 0.75) / size
            pixels[x, y] = (*mix(top, bottom, t), 255)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
    img.putalpha(mask)
    return img


def scaled(points: list[tuple[int, int]], scale: int) -> list[tuple[int, int]]:
    return [(x * scale, y * scale) for x, y in points]


def draw_icon(scale: int = 4) -> Image.Image:
    canvas_size = 256 * scale
    img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    bg = rounded_gradient(canvas_size, 54 * scale)
    img.alpha_composite(bg, (0, 0))
    d = ImageDraw.Draw(img)

    def box(x1: int, y1: int, x2: int, y2: int) -> tuple[int, int, int, int]:
        return (x1 * scale, y1 * scale, x2 * scale, y2 * scale)

    def width(value: int) -> int:
        return max(1, value * scale)

    # Soft decorative sweep.
    d.arc(box(44, 22, 212, 106), 198, 344, fill=(98, 181, 180, 90), width=width(8))

    # Cat shadow.
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.ellipse(box(60, 66, 210, 214), fill=(17, 45, 49, 80))
    shadow = shadow.filter(ImageFilter.GaussianBlur(8 * scale))
    img.alpha_composite(shadow, (0, 8 * scale))
    d = ImageDraw.Draw(img)

    fur_light = (255, 228, 198, 255)
    fur = (248, 192, 145, 255)
    fur_dark = (224, 137, 96, 255)
    inner = (255, 177, 186, 255)
    line = (72, 51, 47, 255)
    mouth = (109, 70, 64, 255)
    whisker = (247, 234, 216, 255)

    # Ears.
    d.polygon(scaled([(70, 103), (74, 47), (134, 84)], scale), fill=fur)
    d.polygon(scaled([(200, 103), (196, 47), (136, 84)], scale), fill=fur)
    d.polygon(scaled([(92, 68), (96, 103), (122, 84)], scale), fill=inner)
    d.polygon(scaled([(178, 68), (174, 103), (148, 84)], scale), fill=inner)

    # Head with a subtle cheek gradient using layered ellipses.
    d.ellipse(box(60, 52, 210, 210), fill=fur)
    d.ellipse(box(69, 58, 201, 198), fill=(251, 202, 160, 255))
    d.ellipse(box(83, 63, 187, 178), fill=fur_light)
    d.ellipse(box(65, 111, 123, 189), fill=(250, 197, 152, 255))
    d.ellipse(box(147, 111, 205, 189), fill=(250, 197, 152, 255))

    # Gentle forehead mark.
    d.rounded_rectangle(box(126, 65, 144, 103), radius=8 * scale, fill=(237, 157, 112, 180))

    # Eyes.
    d.arc(box(91, 119, 127, 146), 20, 160, fill=line, width=width(7))
    d.arc(box(143, 119, 179, 146), 20, 160, fill=line, width=width(7))

    # Nose and mouth.
    d.pieslice(box(127, 145, 143, 163), 200, 340, fill=(216, 107, 114, 255))
    d.pieslice(box(127, 145, 143, 163), 20, 160, fill=(216, 107, 114, 255))
    d.polygon(scaled([(127, 153), (143, 153), (135, 163)], scale), fill=(216, 107, 114, 255))
    d.line(scaled([(135, 162), (128, 170), (116, 172)], scale), fill=mouth, width=width(5), joint="curve")
    d.line(scaled([(135, 162), (142, 170), (154, 172)], scale), fill=mouth, width=width(5), joint="curve")

    # Whiskers.
    for y1, y2 in [(147, 141), (163, 166)]:
        d.line(scaled([(84, y1), (40, y2)], scale), fill=whisker, width=width(5))
        d.line(scaled([(186, y1), (230, y2)], scale), fill=whisker, width=width(5))

    # Highlights.
    d.ellipse(box(98, 108, 112, 122), fill=(255, 242, 220, 190))
    d.ellipse(box(160, 108, 174, 122), fill=(255, 242, 220, 190))

    # Sparkle and dot accents.
    sparkle = scaled([(205, 55), (211, 68), (224, 74), (211, 80), (205, 93), (199, 80), (186, 74), (199, 68)], scale)
    d.polygon(sparkle, fill=(255, 230, 167, 255))
    d.ellipse(box(45, 198, 59, 212), fill=(157, 227, 220, 210))

    return img.resize((256, 256), Image.Resampling.LANCZOS)


def main() -> None:
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    icon = draw_icon()
    icon.save(PNG_PATH)
    icon.save(
        ICO_PATH,
        format="ICO",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    print(f"wrote {PNG_PATH}")
    print(f"wrote {ICO_PATH}")


if __name__ == "__main__":
    main()
