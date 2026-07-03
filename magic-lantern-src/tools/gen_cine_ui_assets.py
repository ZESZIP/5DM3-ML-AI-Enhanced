#!/usr/bin/env python3
"""Generate grayscale BMP assets for Cinema OS 2026 UI (720x400 bg + crystal caps)."""
import os
import struct

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "cine_ui")


def write_bmp_8bit(path, width, height, pixels, palette=None):
    """pixels: row-major, bottom-up BMP order (we pass top-down rows reversed)."""
    if palette is None:
        palette = [(i, i, i) for i in range(256)]

    row_pad = (4 - (width % 4)) % 4
    image_size = (width + row_pad) * height
    palette_size = 256 * 4
    hdr_size = 14 + 40 + palette_size
    file_size = hdr_size + image_size

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<2sIHHI", b"BM", file_size, 0, 0, hdr_size))
        f.write(struct.pack("<IiiHHIIiiII",
            40, width, height, 1, 8, 0, image_size, 2835, 2835, 256, 0))
        for r, g, b in palette:
            f.write(struct.pack("BBB", b, g, r) + b"\x00")
        for y in range(height - 1, -1, -1):
            row = pixels[y * width:(y + 1) * width]
            f.write(bytes(row))
            f.write(b"\x00" * row_pad)


def gen_low_poly_bg(w=720, h=400):
    pixels = [0] * (w * h)
    # Simple low-poly mountain silhouette: layered triangles via scanline fills
    peaks = [(w * 0.12, h * 0.55, 180), (w * 0.35, h * 0.38, 220), (w * 0.58, h * 0.48, 200),
             (w * 0.78, h * 0.32, 240), (w * 0.92, h * 0.50, 170)]
    for y in range(h):
        for x in range(w):
            base = 30 + int(40 * y / h)
            lift = 0
            for px, py, amp in peaks:
                dx = abs(x - px) / (w * 0.18)
                if dx < 1.0:
                    lift = max(lift, int(amp * (1.0 - dx)))
            # Facet quantization for low-poly look
            v = base + lift
            v = (v // 18) * 18 + (v % 18) // 6 * 6
            pixels[y * w + x] = min(255, max(0, v))
    return pixels


def gen_crystal_cap(w=32, h=56, left=True):
    pixels = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            # Trapezoid / faceted end cap
            t = y / max(1, h - 1)
            if left:
                edge = int(w * (0.15 + 0.85 * t))
                facet = (x + y // 4) % 12
            else:
                edge = int(w * (0.85 - 0.85 * t))
                facet = (x + y // 4) % 12

            if left and x >= edge:
                continue
            if not left and x <= edge:
                continue

            rim = (x < 3 or x >= w - 3 or y < 3 or y >= h - 3)
            body = 80 + facet * 8 + int(60 * (1.0 - abs(y - h / 2) / (h / 2)))
            if rim:
                body = min(255, body + 90)
            pixels[y * w + x] = min(255, max(0, body))
    return pixels


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    write_bmp_8bit(os.path.join(OUT_DIR, "low_poly_bg.bmp"), 720, 400, gen_low_poly_bg())
    write_bmp_8bit(os.path.join(OUT_DIR, "highlight_left_cap.bmp"), 32, 56, gen_crystal_cap(left=True))
    write_bmp_8bit(os.path.join(OUT_DIR, "highlight_right_cap.bmp"), 32, 56, gen_crystal_cap(left=False))
    print("Wrote assets to", OUT_DIR)


if __name__ == "__main__":
    main()
