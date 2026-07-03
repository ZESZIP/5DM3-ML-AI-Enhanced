#!/usr/bin/env python3
"""Generate Cinema OS UI BMP assets (720x480 mountain bg + crystal bar sprites)."""
import math
import os
import struct

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "cine_ui")


def write_bmp_8bit(path, width, height, pixels):
    palette = [(i, i, i) for i in range(256)]
    row_pad = (4 - (width % 4)) % 4
    image_size = (width + row_pad) * height
    palette_size = 256 * 4
    hdr_size = 14 + 40 + palette_size
    file_size = hdr_size + image_size
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<2sIHHI", b"BM", file_size, 0, 0, hdr_size))
        f.write(struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, image_size, 2835, 2835, 256, 0))
        for r, g, b in palette:
            f.write(struct.pack("BBB", b, g, r) + b"\x00")
        for y in range(height - 1, -1, -1):
            row = pixels[y * width:(y + 1) * width]
            f.write(bytes(row))
            f.write(b"\x00" * row_pad)


def _edge(a, b, px, py):
    return (px - a[0]) * (b[1] - a[1]) - (py - a[1]) * (b[0] - a[0])


def _in_tri(px, py, t):
    a, b, c, _lum = t
    w1 = _edge(a, b, px, py)
    w2 = _edge(b, c, px, py)
    w3 = _edge(c, a, px, py)
    has_neg = (w1 < 0) or (w2 < 0) or (w3 < 0)
    has_pos = (w1 > 0) or (w2 > 0) or (w3 > 0)
    return not (has_neg and has_pos)


def gen_low_poly_bg(w=720, h=480):
    """Dramatic burnt-orange mountain facets (grayscale for runtime tinting)."""
    pixels = [0] * (w * h)
    horizon = int(h * 0.42)

    # Mountain mesh — sharp low-poly facets
    tris = [
        ((0, horizon), (w * 0.22, h * 0.38), (w * 0.42, horizon), 55),
        ((w * 0.12, horizon), (w * 0.38, h * 0.28), (w * 0.58, horizon), 95),
        ((w * 0.35, horizon), (w * 0.55, h * 0.22), (w * 0.78, horizon), 130),
        ((w * 0.52, horizon), (w * 0.72, h * 0.30), (w, horizon), 85),
        ((0, horizon), (w * 0.55, h * 0.45), (w, horizon), 42),
        ((w * 0.08, horizon), (w * 0.28, h * 0.52), (w * 0.48, horizon), 38),
        ((w * 0.40, horizon), (w * 0.62, h * 0.48), (w * 0.88, horizon), 48),
        ((0, h), (0, horizon), (w, horizon), 25),
        ((0, h), (w, horizon), (w, h), 20),
    ]

    for y in range(h):
        sky = max(6, 22 - int(14 * y / max(1, horizon)))
        for x in range(w):
            if y < horizon:
                pixels[y * w + x] = sky
                continue
            best = 22
            for t in tris:
                if _in_tri(x, y, t):
                    best = max(best, t[3])
            # Facet noise
            facet = ((x // 14) ^ (y // 11)) & 3
            v = best + facet * 6 + int(8 * (y - horizon) / max(1, h - horizon))
            pixels[y * w + x] = min(255, max(0, v))
    return pixels


def gen_crystal_cap(w=56, h=52, left=True):
    pixels = [0] * (w * h)
    cx = 0 if left else w - 1
    for y in range(h):
        t = y / max(1, h - 1)
        for x in range(w):
            if left:
                tip_x = int(w * (0.05 + 0.92 * t))
                if x > tip_x:
                    continue
                depth = (tip_x - x) / max(1, tip_x)
            else:
                tip_x = int(w * (0.95 - 0.92 * t))
                if x < tip_x:
                    continue
                depth = (x - tip_x) / max(1, w - tip_x)

            rim = x < 3 or x >= w - 3 or y < 2 or y >= h - 2
            facet = ((x * 3 + y * 5) // 7) % 5
            body = int(50 + depth * 120 + facet * 14)
            if rim:
                body = min(255, body + 100)
            if y < h * 0.15:
                body = min(255, body + 40)
            pixels[y * w + x] = min(255, max(0, body))
    return pixels


def gen_crystal_mid(w=64, h=52):
    """Repeatable center strip — bronze bar with recessed channel."""
    pixels = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            rim = y < 3 or y >= h - 3
            recess = 10 < y < h - 10
            facet = (x // 8 + y // 6) % 4
            v = 70 + facet * 12
            if rim:
                v = min(255, v + 90)
            if recess and 8 < x < w - 8:
                v = max(12, v - 55)
            if y < 5:
                v = min(255, v + 30)
            pixels[y * w + x] = min(255, max(0, v))
    return pixels


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    write_bmp_8bit(os.path.join(OUT_DIR, "low_poly_bg.bmp"), 720, 480, gen_low_poly_bg())
    write_bmp_8bit(os.path.join(OUT_DIR, "highlight_left_cap.bmp"), 56, 52, gen_crystal_cap(left=True))
    write_bmp_8bit(os.path.join(OUT_DIR, "highlight_right_cap.bmp"), 56, 52, gen_crystal_cap(left=False))
    write_bmp_8bit(os.path.join(OUT_DIR, "highlight_mid.bmp"), 64, 52, gen_crystal_mid())
    print("Wrote Cinema UI assets to", OUT_DIR)


if __name__ == "__main__":
    main()
