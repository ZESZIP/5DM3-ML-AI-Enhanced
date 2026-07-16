"""CSP file read/write."""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Iterable

import numpy as np

from . import codec
from .crc import crc32


def pack_header(
    width: int,
    height: int,
    fps_num: int,
    fps_den: int,
    bit_depth: int,
    compression: int,
    mode_id: str,
    log_tag: int = 1,
    flags: int = 0x4 | 0x8,  # LOSSLESS_I | HAS_INDEX
    black: int = 2047,
    white: int = 15000,
    iso: int = 400,
    shutter_us: int = 40000,
    audio_rate: int = 0,
    audio_ch: int = 0,
    audio_bits: int = 0,
    color_matrix: list[int] | None = None,
) -> bytes:
    buf = bytearray(256)
    buf[0:4] = b"CSP1"
    struct.pack_into("<HH", buf, 4, 1, 256)
    struct.pack_into("<HH", buf, 8, width, height)
    struct.pack_into("<HH", buf, 12, fps_num, fps_den)
    buf[16] = bit_depth & 0xFF
    buf[17] = 0  # RGGB
    buf[18] = compression & 0xFF
    buf[19] = log_tag & 0xFF
    struct.pack_into("<I", buf, 20, flags)
    struct.pack_into("<HH", buf, 24, black, white)
    struct.pack_into("<II", buf, 28, iso, shutter_us)
    cm = color_matrix or [0] * 9
    for i, v in enumerate(cm[:9]):
        struct.pack_into("<i", buf, 36 + i * 4, int(v))
    mid = mode_id.encode("ascii")[:31]
    buf[72 : 72 + len(mid)] = mid
    struct.pack_into("<Q", buf, 108, int(time.time()))
    struct.pack_into("<IHH", buf, 116, audio_rate, audio_ch, audio_bits)
    return bytes(buf)


def unpack_header(data: bytes) -> dict:
    if len(data) < 256 or data[0:4] != b"CSP1":
        raise ValueError("not a CSP1 file")
    version, hsize = struct.unpack_from("<HH", data, 4)
    width, height = struct.unpack_from("<HH", data, 8)
    fps_num, fps_den = struct.unpack_from("<HH", data, 12)
    flags = struct.unpack_from("<I", data, 20)[0]
    black, white = struct.unpack_from("<HH", data, 24)
    iso, shutter = struct.unpack_from("<II", data, 28)
    mode_id = data[72:104].split(b"\x00", 1)[0].decode("ascii", errors="replace")
    return {
        "version": version,
        "header_size": hsize,
        "width": width,
        "height": height,
        "fps_num": fps_num,
        "fps_den": fps_den,
        "bit_depth": data[16],
        "bayer": data[17],
        "compression": data[18],
        "log_tag": data[19],
        "flags": flags,
        "black_level": black,
        "white_level": white,
        "iso": iso,
        "shutter_us": shutter,
        "mode_id": mode_id,
        "audio_rate": struct.unpack_from("<I", data, 116)[0],
    }


@dataclass
class FrameRecord:
    index: int
    timestamp_us: int
    frame_type: int
    quant: int
    payload: bytes
    offset: int = 0


@dataclass
class CspWriter:
    path: Path
    width: int
    height: int
    fps_num: int
    fps_den: int = 1
    compression: int = codec.COMP_SPATIAL
    mode_id: str = "CUSTOM"
    target_mibs: int = 95
    _fh: BinaryIO | None = field(default=None, init=False, repr=False)
    _enc: codec.Encoder | None = field(default=None, init=False, repr=False)
    _index: list[tuple[int, int, int]] = field(default_factory=list, init=False)
    _n: int = field(default=0, init=False)

    def open(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._fh = open(self.path, "wb")
        hdr = pack_header(
            self.width,
            self.height,
            self.fps_num,
            self.fps_den,
            12,
            self.compression,
            self.mode_id,
        )
        self._fh.write(hdr)
        self._enc = codec.Encoder(
            width=self.width,
            height=self.height,
            compression=self.compression,
            budget_bytes=codec.budget_bytes(self.target_mibs, self.fps_num, self.fps_den),
        )

    def write_frame(self, raw: np.ndarray) -> dict:
        assert self._fh and self._enc
        payload, ftype, quant = self._enc.compress(raw, self._n)
        offset = self._fh.tell()
        ts = int(self._n * 1_000_000 * self.fps_den / max(1, self.fps_num))
        head = struct.pack(
            "<4sIQBBHII",
            b"CSFR",
            self._n,
            ts,
            ftype,
            quant,
            0,
            len(payload),
            crc32(payload),
        )
        self._fh.write(head)
        self._fh.write(payload)
        self._index.append((offset, len(payload), ftype))
        info = {
            "index": self._n,
            "bytes": len(payload),
            "type": ftype,
            "quant": quant,
            "mib_s": len(payload) * self.fps_num / self.fps_den / (1024 * 1024),
        }
        self._n += 1
        return info

    def close(self) -> None:
        if not self._fh:
            return
        # Trailer index
        self._fh.write(b"CSIX")
        self._fh.write(struct.pack("<I", len(self._index)))
        for offset, size, ftype in self._index:
            self._fh.write(struct.pack("<QIB", offset, size, ftype))
            self._fh.write(b"\x00" * 3)
        self._fh.close()
        self._fh = None


@dataclass
class CspReader:
    path: Path
    header: dict = field(default_factory=dict)
    frames: list[FrameRecord] = field(default_factory=list)

    def open(self) -> None:
        data = Path(self.path).read_bytes()
        self.header = unpack_header(data[:256])
        pos = self.header["header_size"]
        enc = codec.Encoder(
            width=self.header["width"],
            height=self.header["height"],
            compression=self.header["compression"],
        )
        self.frames.clear()
        while pos + 28 <= len(data):
            magic = data[pos : pos + 4]
            if magic == b"CSIX":
                break
            if magic != b"CSFR":
                # skip unknown
                break
            index, ts, ftype, quant, _res, size, crc = struct.unpack_from("<IQBBHII", data, pos + 4)
            payload = data[pos + 28 : pos + 28 + size]
            if crc32(payload) != crc:
                raise ValueError(f"CRC mismatch at frame {index}")
            self.frames.append(
                FrameRecord(index, ts, ftype, quant, payload, offset=pos)
            )
            pos += 28 + size
        self._enc = enc

    def decode_all(self) -> Iterable[np.ndarray]:
        enc = codec.Encoder(
            width=self.header["width"],
            height=self.header["height"],
            compression=self.header["compression"],
        )
        for fr in self.frames:
            yield enc.decompress(fr.payload, fr.frame_type, fr.quant)
