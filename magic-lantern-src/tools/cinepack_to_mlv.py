#!/usr/bin/env python3
"""Decode CINEPACK-compressed MLV frames to standard MLV (PC side).

CINEPACK v1: horizontal delta + RLE on 14-bit raw samples.
Walks MLV blocks and rewrites VIDF payloads that carry CNPK headers.

Usage:
  python3 cinepack_to_mlv.py input.mlv output.mlv
"""
import struct
import sys

CINEPACK_MAGIC = 0x4B504E43  # CNPK
MLV_BLOCK_VIDF = 0x56444946  # VIDF
MLV_BLOCK_FILE = 0x46494C45  # FILE
MLV_BLOCK_EOF = 0x454F4600   # EOF\0


def decode_cinepack(payload: bytes) -> bytes:
    if len(payload) < 16:
        raise ValueError("CINEPACK header too short")
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
            pos += 1  # skip zero marker
            for _ in range(run):
                out.append(prev)
            continue
        if b == 0xFE:
            lo = payload[pos]
            hi = payload[pos + 1]
            pos += 2
            d = struct.unpack("<h", bytes([lo, hi]))[0]
        elif b >= 128:
            d = b - 256
        else:
            d = b
        prev = (prev + d) & 0x3FFF
        out.append(prev)
    raw = bytearray()
    for v in out:
        raw += struct.pack("<H", v)
    return bytes(raw), w, h, quality


def iter_mlv_blocks(data: bytes):
    pos = 0
    size = len(data)
    while pos + 8 <= size:
        block_type, block_size = struct.unpack_from("<II", data, pos)
        if block_size < 8:
            break
        end = pos + block_size
        if end > size:
            break
        yield block_type, pos, end, data[pos:end]
        if block_type == MLV_BLOCK_EOF:
            break
        pos = end


def rewrite_mlv(inp: bytes) -> bytes:
    out = bytearray()
    decoded_frames = 0
    for block_type, start, end, chunk in iter_mlv_blocks(inp):
        if block_type == MLV_BLOCK_VIDF and len(chunk) > 32:
            payload = chunk[16:]
            if len(payload) >= 16 and struct.unpack_from("<I", payload, 0)[0] == CINEPACK_MAGIC:
                try:
                    raw, w, h, q = decode_cinepack(payload)
                    new_chunk = bytearray(chunk[:16])
                    new_chunk += raw
                    new_size = len(new_chunk)
                    struct.pack_into("<II", new_chunk, 0, MLV_BLOCK_VIDF, new_size)
                    out += new_chunk
                    decoded_frames += 1
                    continue
                except ValueError as e:
                    print(f"WARN frame @{start:#x}: {e}", file=sys.stderr)
        out += chunk
    print(f"Decoded {decoded_frames} CINEPACK frame(s)")
    return bytes(out)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    inp, outp = sys.argv[1], sys.argv[2]
    with open(inp, "rb") as f:
        data = f.read()
    if struct.unpack_from("<I", data, 0)[0] == CINEPACK_MAGIC:
        raw, w, h, q = decode_cinepack(data)
        with open(outp, "wb") as f:
            f.write(raw)
        print(f"Standalone CINEPACK {w}x{h} q={q} -> {len(raw)} bytes raw")
        return
    out = rewrite_mlv(data)
    with open(outp, "wb") as f:
        f.write(out)
    print(f"Wrote {len(out)} bytes to {outp}")


if __name__ == "__main__":
    main()
