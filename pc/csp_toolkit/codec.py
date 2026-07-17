"""CSP frame codec (PACK12 / SPATIAL / TEMPORAL) — Python reference."""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .bitstream import BitReader, BitWriter

COMP_PACK12 = 0
COMP_SPATIAL = 1
COMP_TEMPORAL = 2
FRAME_I = 0
FRAME_P = 1


def pack12(samples: np.ndarray) -> bytes:
    s = samples.astype(np.uint16).ravel() & 0x0FFF
    if len(s) & 1:
        s = np.concatenate([s, np.array([0], dtype=np.uint16)])
    out = bytearray((len(s) // 2) * 3)
    o = 0
    for i in range(0, len(s), 2):
        a = int(s[i])
        b = int(s[i + 1])
        out[o] = a & 0xFF
        out[o + 1] = ((a >> 8) & 0x0F) | ((b & 0x0F) << 4)
        out[o + 2] = (b >> 4) & 0xFF
        o += 3
    return bytes(out)


def unpack12(data: bytes, n: int) -> np.ndarray:
    out = np.zeros(n, dtype=np.uint16)
    s = 0
    i = 0
    while i + 1 < n:
        b0, b1, b2 = data[s], data[s + 1], data[s + 2]
        out[i] = b0 | ((b1 & 0x0F) << 8)
        out[i + 1] = ((b1 >> 4) & 0x0F) | (b2 << 4)
        s += 3
        i += 2
    if n & 1:
        b0, b1 = data[s], data[s + 1]
        out[n - 1] = b0 | ((b1 & 0x0F) << 8)
    return out


def _split_bayer(raw: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    r = raw[0::2, 0::2]
    g1 = raw[0::2, 1::2]
    g2 = raw[1::2, 0::2]
    b = raw[1::2, 1::2]
    return r.copy(), g1.copy(), g2.copy(), b.copy()


def _merge_bayer(r, g1, g2, b, shape) -> np.ndarray:
    out = np.zeros(shape, dtype=np.uint16)
    out[0::2, 0::2] = r
    out[0::2, 1::2] = g1
    out[1::2, 0::2] = g2
    out[1::2, 1::2] = b
    return out


def _encode_plane(plane: np.ndarray, quant: int, bw: BitWriter) -> None:
    h, w = plane.shape
    for y in range(h):
        left = 0
        for x in range(w):
            sample = int(plane[y, x]) & 0x0FFF
            pred = left if x else (0 if y == 0 else int(plane[y - 1, x]))
            r = sample - pred
            if quant > 0:
                sign = -1 if r < 0 else 1
                r = sign * (abs(r) >> quant)
            bw.write_signed_eg(r)
            left = sample


def _decode_plane(h: int, w: int, quant: int, br: BitReader) -> np.ndarray:
    plane = np.zeros((h, w), dtype=np.uint16)
    for y in range(h):
        left = 0
        for x in range(w):
            pred = left if x else (0 if y == 0 else int(plane[y - 1, x]))
            r = br.read_signed_eg()
            if quant > 0:
                r <<= quant
            s = max(0, min(4095, pred + r))
            plane[y, x] = s
            left = s
    return plane


def encode_spatial(raw: np.ndarray, quant: int = 0) -> bytes:
    r, g1, g2, b = _split_bayer(raw)
    bw = BitWriter()
    for plane in (r, g1, g2, b):
        _encode_plane(plane, quant, bw)
    return bw.getvalue()


def decode_spatial(data: bytes, shape: tuple[int, int], quant: int = 0) -> np.ndarray:
    h, w = shape
    ph, pw = h // 2, w // 2
    br = BitReader(data)
    r = _decode_plane(ph, pw, quant, br)
    g1 = _decode_plane(ph, pw, quant, br)
    g2 = _decode_plane(ph, pw, quant, br)
    b = _decode_plane(ph, pw, quant, br)
    return _merge_bayer(r, g1, g2, b, shape)


@dataclass
class Encoder:
    width: int
    height: int
    compression: int = COMP_SPATIAL
    gop: int = 12
    quant: int = 0
    q_max: int = 6
    budget_bytes: int = 0
    prev: np.ndarray | None = field(default=None, repr=False)

    def compress(self, raw: np.ndarray, frame_index: int) -> tuple[bytes, int, int]:
        raw = raw.astype(np.uint16) & 0x0FFF
        if self.compression == COMP_PACK12:
            payload = pack12(raw)
            self.prev = raw.copy()
            return payload, FRAME_I, 0

        is_i = True
        if self.compression == COMP_TEMPORAL and self.gop > 0:
            is_i = (frame_index % self.gop) == 0 or self.prev is None

        q = self.quant
        while True:
            if is_i or self.prev is None:
                payload = encode_spatial(raw, q)
                ftype = FRAME_I
            else:
                delta = raw.astype(np.int32) - self.prev.astype(np.int32)
                biased = np.clip(delta + 2048, 0, 4095).astype(np.uint16)
                payload = encode_spatial(biased, q)
                ftype = FRAME_P
            if self.budget_bytes <= 0 or len(payload) <= self.budget_bytes or q >= self.q_max:
                self.prev = raw.copy()
                return payload, ftype, q
            q += 1

    def decompress(self, payload: bytes, frame_type: int, quant: int) -> np.ndarray:
        shape = (self.height, self.width)
        if self.compression == COMP_PACK12:
            flat = unpack12(payload, self.width * self.height)
            frame = flat.reshape(shape)
            self.prev = frame.copy()
            return frame
        if frame_type == FRAME_I or self.prev is None:
            frame = decode_spatial(payload, shape, quant)
        else:
            biased = decode_spatial(payload, shape, quant)
            delta = biased.astype(np.int32) - 2048
            frame = np.clip(self.prev.astype(np.int32) + delta, 0, 4095).astype(np.uint16)
        self.prev = frame.copy()
        return frame


def budget_bytes(target_mibs: int, fps_num: int, fps_den: int = 1) -> int:
    return (target_mibs * 1024 * 1024 * fps_den) // max(1, fps_num)


def debayer_bilinear(raw: np.ndarray) -> np.ndarray:
    """Fast-ish RGGB bilinear demosaic → float RGB normalized to 12-bit full scale."""
    r = raw.astype(np.float64)
    h, w = r.shape
    rgb = np.zeros((h, w, 3), dtype=np.float64)

    rgb[0::2, 0::2, 0] = r[0::2, 0::2]
    rgb[0::2, 1::2, 1] = r[0::2, 1::2]
    rgb[1::2, 0::2, 1] = r[1::2, 0::2]
    rgb[1::2, 1::2, 2] = r[1::2, 1::2]

    # Interpolate R at non-R sites
    R = rgb[:, :, 0]
    G = rgb[:, :, 1]
    B = rgb[:, :, 2]
    # Average 4-neighbors where zero using convolution-like shifts
    def fill(ch: np.ndarray) -> np.ndarray:
        out = ch.copy()
        mask = out == 0
        acc = np.zeros_like(out)
        cnt = np.zeros_like(out)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            shifted = np.roll(np.roll(out, dy, axis=0), dx, axis=1)
            valid = shifted != 0
            acc += shifted * valid
            cnt += valid
        filled = np.divide(acc, np.maximum(cnt, 1), dtype=np.float64)
        out[mask] = filled[mask]
        return out

    # Two passes improve corners
    for _ in range(2):
        rgb[:, :, 0] = fill(rgb[:, :, 0])
        rgb[:, :, 1] = fill(rgb[:, :, 1])
        rgb[:, :, 2] = fill(rgb[:, :, 2])
    return np.clip(rgb / 4095.0, 0.0, 1.0)
