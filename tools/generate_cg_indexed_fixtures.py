#!/usr/bin/env python3
"""Build tiny indexed PNGs; reuse the independently specified P3 ICC only.

No image encoder or CMS supplies expected pixels. The source PLTE entries
match png-samples.h; indices cycle over entries across odd-width rows.
"""
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tests/data/color-numeric"
PALETTE = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (128, 128, 128),
           (13, 77, 219), (255, 255, 255), (0, 0, 0), (103, 151, 197)]


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data +
            struct.pack(">I", zlib.crc32(tag + data)))


def profile_chunk():
    data = (OUT / "png-p3-8-rgb.png").read_bytes()
    pos = 8
    while pos < len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        if data[pos + 4:pos + 8] == b"iCCP":
            return data[pos:pos + length + 12]
        pos += length + 12
    raise ValueError("P3 source fixture has no ICC")


def fixture(bits, srgb=False, alpha=None, adam7=False):
    width, height = 5, 3
    colors = min(8, 1 << bits)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, bits,
                                       3, 0, 0, int(adam7)))
    data += chunk(b"sRGB", b"\0") if srgb else profile_chunk()
    data += chunk(b"PLTE", bytes(v for rgb in PALETTE[:colors] for v in rgb))
    if alpha is not None:
        data += chunk(b"tRNS", bytes(alpha[:colors]))
    raw = bytearray()
    passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8),
              (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
    for x0, y0, dx, dy in passes if adam7 else [(0, 0, 1, 1)]:
        xs = range(x0, width, dx)
        if not xs:
            continue
        for y in range(y0, height, dy):
            row = bytearray((len(xs) * bits + 7) // 8)
            for p, x in enumerate(xs):
                slot = (y * width + x) % colors
                row[p * bits // 8] |= slot << (8 - bits - p * bits % 8)
            raw += b"\0" + row
    data += chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    return data


def main():
    for bits in [1, 2, 4, 8]:
        (OUT / f"cg-indexed-p3-{bits}.png").write_bytes(fixture(bits))
    (OUT / "cg-indexed-srgb.png").write_bytes(fixture(8, srgb=True))
    for srgb in [False, True]:
        space = "srgb" if srgb else "p3"
        for name, alpha in [
                ("binary", [255, 255, 0, 255, 255, 255, 255, 255]),
                ("partial", [255, 128, 0, 255, 64, 255, 255, 193])]:
            (OUT / f"cg-expanded-{space}-{name}.png").write_bytes(
                fixture(8, srgb=srgb, alpha=alpha))
        for bits in ([8] if srgb else [1, 2, 4, 8]):
            (OUT / f"cg-expanded-{space}-adam7-{bits}.png").write_bytes(
                fixture(bits, srgb=srgb, adam7=True))


if __name__ == "__main__":
    main()
