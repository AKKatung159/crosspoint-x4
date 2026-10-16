"""2-bit greyscale BMP, the format the X4 firmware draws natively (0 = black ... 3 = white)."""
from __future__ import annotations

import struct

from PIL import Image

LEVELS = (0, 85, 170, 255)


def quantize(im: Image.Image) -> Image.Image:
    """Nearest of the four panel greys, no dithering (flat UI, crisp text)."""
    lut = [min(range(4), key=lambda i: abs(LEVELS[i] - v)) for v in range(256)]
    return im.convert("L").point(lut)


def preview(im: Image.Image) -> Image.Image:
    """What the panel shows: the four greys as an ordinary L image."""
    return quantize(im).point([LEVELS[i] if i < 4 else 0 for i in range(256)])


def encode(im: Image.Image) -> bytes:
    """`im` (any mode) -> 2 bpp BMP bytes, bottom-up rows, 4-colour grey palette."""
    q = quantize(im)
    w, h = q.size
    stride = ((w * 2 + 31) // 32) * 4
    px = q.load()
    rows = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(stride)
        for x in range(w):
            row[x >> 2] |= px[x, y] << (6 - 2 * (x & 3))
        rows += row
    palette = b"".join(bytes((v, v, v, 0)) for v in LEVELS)
    offset = 14 + 40 + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(rows), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 2, 0, len(rows), 2835, 2835, 4, 4)
    return header + info + palette + bytes(rows)


def decode(data: bytes) -> Image.Image:
    """For tests and the PNG preview: 2 bpp BMP -> L image."""
    offset = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    stride = ((w * 2 + 31) // 32) * 4
    im = Image.new("L", (w, abs(h)))
    px = im.load()
    for r in range(abs(h)):
        y = abs(h) - 1 - r if h > 0 else r
        row = data[offset + r * stride: offset + (r + 1) * stride]
        for x in range(w):
            px[x, y] = LEVELS[(row[x >> 2] >> (6 - 2 * (x & 3))) & 3]
    return im
