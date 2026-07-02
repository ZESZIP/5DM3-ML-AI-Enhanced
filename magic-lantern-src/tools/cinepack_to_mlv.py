#!/usr/bin/env python3
"""Convert CINEPACK .CIX streams or MLV files to standard MLV.

CINEPACK v2: row-wise delta + aggressive zero-run encoding.
CIX: direct-to-card stream from 5D3 Cine AI Enhanced.

Usage:
  python3 cinepack_to_mlv.py input.CIX output.MLV
  python3 cinepack_to_mlv.py input.MLV output.MLV
"""
import struct
import sys

CINEPACK_MAGIC = 0x4B504E43
CIX_MAGIC = 0x31584943
CIX_FRAME_MAGIC = 0x46584943
MLV_BLOCK_VIDF = 0x56444946
MLV_BLOCK_FILE = 0x46494C45
MLV_BLOCK_RAWI = 0x49574152
MLV_BLOCK_MLVI = 0x49564C4D
MLV_BLOCK_EOF = 0x454F4600


def decode_cinepack_v1(payload: bytes):
    magic, ver, w, h = struct.unpack_from("<IHHI", payload, 0)
    if magic != CINEPACK_MAGIC:
        raise ValueError(f"Not CINEPACK (magic {magic:#x})")
    quality = payload[10]
    pos = 16
    out = []
    prev = 0
    while len(out) < w * h and pos < len(payload):
        b = payload[pos]
        pos += 1
        if b == 0xFF:
            run = payload[pos]
            pos += 1
            pos += 1
            for _ in range(run):
                out.append(prev)
            continue
        if b == 0xFE:
            lo, hi = payload[pos], payload[pos + 1]
            pos += 2
            d = struct.unpack("<h", bytes([lo, hi]))[0]
        elif b >= 128:
            d = b - 256
        else:
            d = b
        prev = (prev + d) & 0x3FFF
        out.append(prev)
    return out, w, h, quality


def decode_cinepack_v2(payload: bytes):
    magic, ver, w, h = struct.unpack_from("<IHHI", payload, 0)
    if magic != CINEPACK_MAGIC or ver != 2:
        return decode_cinepack_v1(payload)
    quality = payload[10]
    pos = 16
    out = []
    prev = 0
    row = 0
    while pos < len(payload) and len(out) < w * h:
        b = payload[pos]
        pos += 1
        if b == 0xFD:
            row = payload[pos]
            pos += 1
            prev = 0
            continue
        if b == 0x00:
            run = payload[pos]
            pos += 1
            for _ in range(run):
                out.append(prev)
            continue
        if b == 0xFF:
            tag = payload[pos]
            pos += 1
            if tag == 0x00:
                run = payload[pos] | (payload[pos + 1] << 8)
                pos += 2
                for _ in range(run):
                    out.append(prev)
            elif tag == 0xFE:
                lo, hi = payload[pos], payload[pos + 1]
                pos += 2
                d = struct.unpack("<h", bytes([lo, hi]))[0]
                prev = (prev + d) & 0x3FFF
                out.append(prev)
            continue
        if b >= 128:
            d = b - 256
        else:
            d = b
        prev = (prev + d) & 0x3FFF
        out.append(prev)
    return out, w, h, quality


def decode_cinepack(payload: bytes):
    ver = struct.unpack_from("<H", payload, 4)[0]
    if ver >= 2:
        return decode_cinepack_v2(payload)
    return decode_cinepack_v1(payload)


def pixels_to_raw_bytes(pixels):
    raw = bytearray()
    for v in pixels:
        raw += struct.pack("<H", v)
    return bytes(raw)


def iter_cix_frames(data: bytes):
    if len(data) < 128:
        return
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != CIX_MAGIC:
        return
    pos = 128
    while pos + 16 <= len(data):
        fm, psz, fnum, _ = struct.unpack_from("<IIII", data, pos)
        if fm != CIX_FRAME_MAGIC:
            break
        pos += 16
        payload = data[pos:pos + psz]
        pos += psz
        yield fnum, payload


def cix_to_mlv(data: bytes, outp: str):
    hdr = struct.unpack_from("<IHHIIBBH", data, 0, 20)
    magic, ver, w, h, fps, bpp, codec, target = hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6], hdr[7]
    print(f"CIX {w}x{h} fps={fps/1000:.3f} codec=v{codec} target={target} cMB/s")

    frames = []
    for fnum, payload in iter_cix_frames(data):
        pixels, pw, ph, q = decode_cinepack(payload)
        frames.append((fnum, pixels_to_raw_bytes(pixels)))
        print(f"  frame {fnum}: {len(payload)} -> {len(frames[-1][1])} bytes raw q={q}")

    if not frames:
        raise ValueError("No CIX frames found")

    with open(outp, "wb") as out:
        file_hdr = struct.pack("<IIIIIIII", MLV_BLOCK_FILE, 0x100, 0, 0, 0, 0, 0, len(frames))
        out.write(file_hdr)
        rawi = struct.pack("<IIHHHHHH", MLV_BLOCK_RAWI, 0x40, w, h, bpp, 1, 0, 0)
        out.write(rawi + b"\x00" * (0x40 - len(rawi)))
        for fnum, raw in frames:
            vidf_sz = 64 + len(raw)
            vidf = struct.pack("<IIIIIIII", MLV_BLOCK_VIDF, vidf_sz, 0, fnum, 0, 0, 0, 0)
            out.write(vidf + b"\x00" * (64 - len(vidf)))
            out.write(raw)
        out.write(struct.pack("<II", MLV_BLOCK_EOF, 8))
    print(f"Wrote {len(frames)} frames to {outp}")


def rewrite_mlv(inp: bytes, outp: str):
    out = bytearray()
    decoded = 0
    pos = 0
    size = len(inp)
    while pos + 8 <= size:
        block_type, block_size = struct.unpack_from("<II", inp, pos)
        if block_size < 8:
            break
        end = pos + block_size
        chunk = inp[pos:end]
        if block_type == MLV_BLOCK_VIDF and len(chunk) > 80:
            payload = chunk[64:]
            if len(payload) >= 16 and struct.unpack_from("<I", payload, 0)[0] == CINEPACK_MAGIC:
                pixels, w, h, q = decode_cinepack(payload)
                raw = pixels_to_raw_bytes(pixels)
                new_chunk = bytearray(chunk[:64])
                new_chunk += raw
                struct.pack_into("<II", new_chunk, 0, MLV_BLOCK_VIDF, len(new_chunk))
                out += new_chunk
                decoded += 1
                pos = end
                continue
        out += chunk
        if block_type == MLV_BLOCK_EOF:
            break
        pos = end
    with open(outp, "wb") as f:
        f.write(out)
    print(f"Decoded {decoded} CINEPACK frame(s) -> {outp}")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    inp, outp = sys.argv[1], sys.argv[2]
    with open(inp, "rb") as f:
        data = f.read()
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic == CIX_MAGIC:
        cix_to_mlv(data, outp)
    elif magic == CINEPACK_MAGIC:
        pixels, w, h, q = decode_cinepack(data)
        with open(outp, "wb") as f:
            f.write(pixels_to_raw_bytes(pixels))
        print(f"Standalone CNPK {w}x{h} -> {outp}")
    else:
        rewrite_mlv(data, outp)


if __name__ == "__main__":
    main()
