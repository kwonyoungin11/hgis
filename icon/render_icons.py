"""Render ka-hgis app icons into ./icon (no app integration)."""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent if "__file__" in dir() else Path(r"D:\qgis\icon")
OUT = Path(r"D:\qgis\icon")
OUT.mkdir(parents=True, exist_ok=True)


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def mix(c1, c2, t: float):
    return tuple(int(lerp(a, b, t)) for a, b in zip(c1, c2))


def draw_icon(size: int) -> Image.Image:
    s = float(size)
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # Shadow
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    m = s * 0.07
    sd.rounded_rectangle(
        (m + s * 0.02, m + s * 0.03, s - m + s * 0.02, s - m + s * 0.03),
        radius=s * 0.2,
        fill=(0, 0, 0, 55),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=max(1, int(s * 0.035))))
    img = Image.alpha_composite(img, shadow)
    d = ImageDraw.Draw(img)

    # Background tile with diagonal gradient
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    td = ImageDraw.Draw(tile)
    pad = s * 0.065
    box = (pad, pad, s - pad, s - pad)
    # fill via pixels for gradient inside rounded rect mask
    grad = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    for y in range(size):
        for x in range(size):
            t = (x + y) / (2 * (s - 1)) if s > 1 else 0
            if t < 0.55:
                c = mix((15, 61, 76), (31, 111, 120), t / 0.55)
            else:
                c = mix((31, 111, 120), (139, 90, 43), (t - 0.55) / 0.45)
            grad.putpixel((x, y), (*c, 255))
    mask = Image.new("L", (size, size), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle(box, radius=s * 0.19, fill=255)
    tile = Image.composite(grad, tile, mask)
    img = Image.alpha_composite(img, tile)
    d = ImageDraw.Draw(img)

    # Grid
    gx0, gy0, gx1, gy1 = s * 0.22, s * 0.24, s * 0.78, s * 0.76
    grid_col = (232, 244, 242, 36)
    for i in range(4):
        y = gy0 + (gy1 - gy0) * (i + 1) / 5
        d.line([(gx0, y), (gx1, y)], fill=grid_col, width=max(1, int(s * 0.006)))
    for i in range(3):
        x = gx0 + (gx1 - gx0) * (i + 1) / 4
        d.line([(x, gy0), (x, gy1)], fill=grid_col, width=max(1, int(s * 0.006)))

    # Map sheet
    sheet = [
        (s * 0.25, s * 0.23),
        (s * 0.70, s * 0.23),
        (s * 0.77, s * 0.30),
        (s * 0.77, s * 0.70),
        (s * 0.72, s * 0.75),
        (s * 0.25, s * 0.75),
        (s * 0.20, s * 0.70),
        (s * 0.20, s * 0.28),
    ]
    d.polygon(sheet, fill=(240, 236, 224, 250), outline=(44, 36, 24, 60))
    # fold
    d.polygon(
        [(s * 0.70, s * 0.23), (s * 0.77, s * 0.30), (s * 0.725, s * 0.30), (s * 0.70, s * 0.27)],
        fill=(185, 168, 138, 255),
    )

    # Excavation polygon (soil)
    trench = [
        (s * 0.33, s * 0.49),
        (s * 0.485, s * 0.41),
        (s * 0.62, s * 0.46),
        (s * 0.585, s * 0.61),
        (s * 0.41, s * 0.645),
    ]
    d.polygon(trench, fill=(180, 120, 62, 255), outline=(92, 58, 30, 255))
    # feature
    feat = [
        (s * 0.41, s * 0.50),
        (s * 0.51, s * 0.465),
        (s * 0.56, s * 0.525),
        (s * 0.485, s * 0.58),
    ]
    d.polygon(feat, fill=(231, 194, 122, 255), outline=(107, 74, 31, 255))

    # GCP / crosshair
    cx, cy, r = s * 0.62, s * 0.39, s * 0.055
    d.ellipse((cx - r * 1.35, cy - r * 1.35, cx + r * 1.35, cy + r * 1.35), fill=(11, 58, 68, 235))
    d.ellipse((cx - r * 0.55, cy - r * 0.55, cx + r * 0.55, cy + r * 0.55), fill=(242, 193, 78, 255))
    arm = r * 1.85
    w = max(1, int(s * 0.016))
    gold = (242, 193, 78, 255)
    d.line([(cx, cy - arm), (cx, cy - r * 0.9)], fill=gold, width=w)
    d.line([(cx, cy + r * 0.9), (cx, cy + arm)], fill=gold, width=w)
    d.line([(cx - arm, cy), (cx - r * 0.9, cy)], fill=gold, width=w)
    d.line([(cx + r * 0.9, cy), (cx + arm, cy)], fill=gold, width=w)

    # North tick
    n = [
        (s * 0.29, s * 0.30),
        (s * 0.315, s * 0.365),
        (s * 0.29, s * 0.345),
        (s * 0.265, s * 0.365),
    ]
    d.polygon(n, fill=(31, 111, 120, 255))

    # Top highlight arc approximation
    hi = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    hd = ImageDraw.Draw(hi)
    hd.arc(
        (s * 0.12, s * 0.08, s * 0.88, s * 0.55),
        start=200,
        end=340,
        fill=(255, 255, 255, 40),
        width=max(2, int(s * 0.035)),
    )
    hi = hi.filter(ImageFilter.GaussianBlur(radius=max(1, int(s * 0.01))))
    img = Image.alpha_composite(img, hi)
    return img


def save_ico(images: dict[int, Image.Image], path: Path) -> None:
    # Windows ICO with multiple sizes
    sizes = sorted(images.keys())
    images[sizes[-1]].save(
        path,
        format="ICO",
        sizes=[(n, n) for n in sizes if n <= 256],
        append_images=[images[n] for n in sizes if n < sizes[-1] and n <= 256],
    )


def main() -> None:
    sizes = [16, 24, 32, 48, 64, 128, 256, 512]
    rendered: dict[int, Image.Image] = {}
    for n in sizes:
        im = draw_icon(n)
        rendered[n] = im
        out = OUT / f"ka-hgis-{n}.png"
        im.save(out, format="PNG")
        print("wrote", out)

    master = OUT / "ka-hgis-icon.png"
    rendered[512].save(master, format="PNG")
    print("wrote", master)

    # Prefer common ICO sizes
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_imgs = [rendered[n] for n in ico_sizes]
    ico_path = OUT / "ka-hgis.ico"
    ico_imgs[-1].save(ico_path, format="ICO", sizes=[(n, n) for n in ico_sizes])
    print("wrote", ico_path)

    readme = OUT / "README.md"
    readme.write_text(
        """# ka-hgis icons

App icon candidates for **ka-hgis** (field archaeology HGIS).

| File | Use |
|------|-----|
| `ka-hgis-icon.svg` | Vector source |
| `ka-hgis-icon.png` | Master 512×512 |
| `ka-hgis-16.png` … `ka-hgis-512.png` | Fixed sizes |
| `ka-hgis.ico` | Windows multi-size icon |

## Design

- Teal → earth gradient tile (GIS + soil)
- Map sheet + excavation polygon
- Gold control-point / GPS crosshair
- Not wired into the app yet (assets only)

## Regenerate PNGs

```powershell
python icon\\render_icons.py
```
""",
        encoding="utf-8",
    )
    print("wrote", readme)


if __name__ == "__main__":
    main()
