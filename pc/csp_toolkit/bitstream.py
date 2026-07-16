"""Exp-Golomb bit packer/unpacker for CSP spatial coding."""

from __future__ import annotations


class BitWriter:
    def __init__(self) -> None:
        self._bytes = bytearray()
        self._cur = 0
        self._bits = 0

    def write_bit(self, bit: int) -> None:
        self._cur = (self._cur << 1) | (1 if bit else 0)
        self._bits += 1
        if self._bits == 8:
            self._bytes.append(self._cur & 0xFF)
            self._cur = 0
            self._bits = 0

    def write_bits(self, value: int, n: int) -> None:
        for i in range(n - 1, -1, -1):
            self.write_bit((value >> i) & 1)

    def write_eg(self, value: int) -> None:
        x = value + 1
        n = x.bit_length()
        zeros = n - 1
        for _ in range(zeros):
            self.write_bit(0)
        self.write_bits(x, n)

    def write_signed_eg(self, value: int) -> None:
        u = (-2 * value) if value <= 0 else (2 * value - 1)
        self.write_eg(u)

    def getvalue(self) -> bytes:
        if self._bits:
            self._bytes.append((self._cur << (8 - self._bits)) & 0xFF)
            self._cur = 0
            self._bits = 0
        return bytes(self._bytes)


class BitReader:
    def __init__(self, data: bytes) -> None:
        self._data = data
        self._pos = 0
        self._bit = 0

    def read_bit(self) -> int:
        if self._pos >= len(self._data):
            raise EOFError("bitstream exhausted")
        bit = (self._data[self._pos] >> (7 - self._bit)) & 1
        self._bit += 1
        if self._bit == 8:
            self._bit = 0
            self._pos += 1
        return bit

    def read_eg(self) -> int:
        zeros = 0
        while True:
            b = self.read_bit()
            if b == 1:
                break
            zeros += 1
            if zeros > 31:
                raise ValueError("invalid exp-golomb")
        x = 1
        for _ in range(zeros):
            x = (x << 1) | self.read_bit()
        return x - 1

    def read_signed_eg(self) -> int:
        u = self.read_eg()
        if u & 1:
            return (u + 1) // 2
        return -(u // 2)
