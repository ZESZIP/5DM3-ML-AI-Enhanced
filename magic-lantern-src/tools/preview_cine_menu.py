#!/usr/bin/env python3
"""Preview Cinema OS menu layout (720x480) — verify against reference mockup."""
import os
import struct
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Install Pillow: pip install Pillow")
    sys.exit(1)

ROOT = os.path.join(os.path.dirname(__file__), "..")
ASSET_DIR = os.path.join(ROOT, "data", "cine_ui")
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "cine_menu_preview.png")

# Reference palette
ORANGE = (255, 102, 0)
YELLOW = (255, 204, 0)
GREEN = (76, 217, 100)
BLUE = (0, 162, 255)
PURPLE = (165, 94, 234)
WHITE = (255, 255, 255)
DARK = (12, 10, 8)
BAR_BRONZE = (180, 90, 40)


def load_bmp_gray(path):
    with open(path, "rb") as f:
        f.read(14)
        hdr = f.read(40)
        w, h = struct.unpack_from("<ii", hdr, 4)
        f.read(256 * 4)
        pad = (4 - w % 4) % 4
        rows = []
        for _ in range(h):
            rows.append(list(f.read(w)))
            f.read(pad)
    rows.reverse()
    flat = [p for row in rows for p in row]
    return w, h, flat


def tint_bg(w, h, gray, accent):
    img = Image.new("RGB", (w, h))
    px = img.load()
    ar, ag, ab = accent
    for y in range(h):
        for x in range(w):
            g = gray[y * w + x]
            if g < 6:
                px[x, y] = (0, 0, 0)
            else:
                t = g / 255.0
                px[x, y] = (int(ar * t * 0.55), int(ag * t * 0.35), int(ab * t * 0.15 + 8 * t))
    return img


def draw_crystal_bar(draw, x, y, w, h):
    draw.polygon([(x, y + h // 2), (x + 22, y), (x + w - 22, y), (x + w, y + h // 2),
                  (x + w - 22, y + h), (x + 22, y + h)], fill=BAR_BRONZE, outline=(255, 180, 80))
    draw.rectangle([x + 28, y + 8, x + w - 28, y + h - 8], fill=(20, 12, 8))


def main():
    bg_path = os.path.join(ASSET_DIR, "low_poly_bg.bmp")
    if not os.path.isfile(bg_path):
        os.system(f"{sys.executable} {os.path.join(os.path.dirname(__file__), 'gen_cine_ui_assets.py')}")
    bw, bh, gray = load_bmp_gray(bg_path)
    img = tint_bg(bw, bh, gray, ORANGE)
    draw = ImageDraw.Draw(img)

    # Status header
    draw.rectangle([0, 0, 720, 32], fill=(0, 0, 0, 180))
    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
        font_sm = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12)
    except OSError:
        font = font_sm = ImageFont.load_default()
    draw.text((16, 8), "85%", fill=WHITE, font=font_sm)
    draw.text((340, 6), "MENU", fill=WHITE, font=font)
    draw.text((620, 8), "14:32", fill=WHITE, font=font_sm)

    # Nav tabs
    draw.rectangle([0, 32, 720, 76], fill=(0, 0, 0))
    tabs = [("SETTINGS", YELLOW), ("PHOTO", GREEN), ("CINE", ORANGE), ("ADD-ONS", BLUE), ("HACKS", PURPLE)]
    tw = 720 // 5
    for i, (name, col) in enumerate(tabs):
        tx = i * tw
        c = col if name == "CINE" else (140, 140, 140)
        draw.text((tx + 36, 42), name, fill=c, font=font_sm)
        if name == "CINE":
            draw.rectangle([tx + 4, 70, tx + tw - 4, 74], fill=ORANGE)

    rows = [
        ("LUT", "RAW", False),
        ("Resolution", "UHD 4160x2560 [4K]", True),
        ("FRAME RATE", "25P", False),
        ("CODEC", "MOV", False),
        ("AUDIO", "STEREO", False),
        ("ASPECT RATIO", "1.85:1", False),
    ]
    y0 = 92
    rh = 44
    for i, (label, val, sel) in enumerate(rows):
        y = y0 + i * rh
        if sel:
            draw_crystal_bar(draw, 10, y - 2, 700, 50)
            draw.text((56, y + 10), label, fill=WHITE, font=font)
            draw.text((480, y + 14), val, fill=WHITE, font=font_sm)
        else:
            draw.text((24, y + 12), label, fill=WHITE, font=font_sm)
            draw.text((480, y + 14), val, fill=WHITE, font=font_sm)

    # Scrollbar
    draw.rectangle([712, y0, 716, y0 + 6 * rh], fill=(40, 30, 20))

    # Footer
    draw.rectangle([0, 452, 720, 480], fill=(0, 0, 0))
    draw.text((16, 458), "85%", fill=WHITE, font=font_sm)
    draw.text((340, 458), "14:32", fill=WHITE, font=font_sm)
    draw.text((560, 458), "CFast A: 128GB", fill=WHITE, font=font_sm)

    img.save(OUT)
    print("Preview saved:", OUT)


if __name__ == "__main__":
    main()
