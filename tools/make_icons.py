"""Generate extension icons for Sensory Shield (transparent background, shield glyph)."""

import os
from PIL import Image, ImageDraw

OUT = "/Users/kedewei/Library/Application Support/com.tencent.mac.marvis/MarvisData/User/oAN1i2Q4OggTWXenZkJ7hAAavTR0/workspace/conv_70e6d88f2a0c431b94bc7353cd2e1980/temp/sensory-shield/icons"
os.makedirs(OUT, exist_ok=True)

BRAND = (43, 122, 120, 255)      # #2B7A78 calm teal
BRAND_DARK = (28, 86, 84, 255)
INK = (43, 43, 43, 255)
SAND = (231, 224, 210, 255)      # #E7E0D2

SS = 8  # supersampling factor


def shield_points(size, pad_ratio=0.10):
    p = size * pad_ratio
    w = size - 2 * p
    cx = size / 2
    top = p
    shoulder = p + w * 0.16
    upper = p + w * 0.52
    bottom = size - p
    return [
        (cx, top),
        (size - p, shoulder),
        (size - p, upper),
        (cx, bottom),
        (p, upper),
        (p, shoulder),
    ]


def render(size):
    S = size * SS
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    pts = shield_points(S)
    d.polygon(pts, fill=BRAND)

    # Inner calm lines (three horizontal strokes, centered, rounded)
    if size >= 32:
        lw = max(SS * 2, int(S * 0.055))
        for i, ratio in enumerate((0.34, 0.46, 0.58)):
            y = S * ratio
            half = S * (0.20 if i < 2 else 0.13)
            d.line([(S / 2 - half, y), (S / 2 + half, y)], fill=SAND, width=lw)
    else:
        lw = max(SS, int(S * 0.09))
        d.line([(S * 0.32, S * 0.36), (S * 0.68, S * 0.36)], fill=SAND, width=lw)
        d.line([(S * 0.32, S * 0.52), (S * 0.68, S * 0.52)], fill=SAND, width=lw)

    return img.resize((size, size), Image.LANCZOS)


for sz in (16, 32, 48, 128):
    render(sz).save(os.path.join(OUT, f"icon{sz}.png"))

# Store listing tile (1280x800) for reference, not required by the manifest
tile = Image.new("RGBA", (1280, 800), SAND)
td = ImageDraw.Draw(tile)
td.rounded_rectangle([80, 120, 1200, 680], radius=48, fill=(255, 255, 255, 255))
big = render(320)
tile.alpha_composite(big, (160, 260))
tile.convert("RGB").save("/Users/kedewei/Library/Application Support/com.tencent.mac.marvis/MarvisData/User/oAN1i2Q4OggTWXenZkJ7hAAavTR0/workspace/conv_70e6d88f2a0c431b94bc7353cd2e1980/temp/sensory-shield-store-tile.png")

print("icons written:", sorted(os.listdir(OUT)))
for sz in (16, 32, 48, 128):
    with Image.open(os.path.join(OUT, f"icon{sz}.png")) as im:
        print(f"icon{sz}.png {im.size} {im.mode} {os.path.getsize(os.path.join(OUT, f'icon{sz}.png'))} bytes")
