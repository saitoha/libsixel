#!/usr/bin/env python3
"""Generate deterministic PNG fixtures for palette-pipeline measurements."""

from __future__ import annotations

import argparse
import colorsys
import hashlib
import struct
import sys
import zlib
from pathlib import Path
from typing import Callable, Dict, Iterable, Tuple


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PROFILE_SOURCE = Path(
    "tests/data/inputs/snake_64_embedded_a98_icc.webp"
)
FIXTURE_DIRECTORY = Path("images/measurements/palette-pipeline")
PixelFunction = Callable[[int, int, int, int], Tuple[int, int, int]]


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    """Return one checksummed PNG chunk."""
    length = struct.pack(">I", len(payload))
    checksum = struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    return length + kind + payload + checksum


def adler32(data: bytes) -> int:
    """Return the RFC 1950 Adler-32 value without a zlib dependency."""
    first = 1
    second = 0
    modulus = 65521
    for value in data:
        first = (first + value) % modulus
        second = (second + first) % modulus
    return (second << 16) | first


def zlib_stored(data: bytes) -> bytes:
    """Return a canonical zlib stream made only of stored DEFLATE blocks."""
    stream = bytearray(b"\x78\x01")
    if not data:
        stream.extend(b"\x01\x00\x00\xff\xff")
    for offset in range(0, len(data), 65535):
        block = data[offset:offset + 65535]
        final = offset + len(block) == len(data)
        stream.append(1 if final else 0)
        stream.extend(struct.pack("<HH", len(block), len(block) ^ 0xFFFF))
        stream.extend(block)
    stream.extend(struct.pack(">I", adler32(data)))
    return bytes(stream)


def extract_webp_icc(path: Path) -> bytes:
    """Extract the ICCP payload from a RIFF WebP fixture."""
    data = path.read_bytes()
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WEBP":
        raise ValueError(f"not a RIFF WebP file: {path}")
    offset = 12
    while offset + 8 <= len(data):
        kind = data[offset:offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        start = offset + 8
        end = start + size
        if end > len(data):
            raise ValueError(f"truncated WebP chunk in {path}")
        if kind == b"ICCP":
            if size == 0:
                raise ValueError(f"empty ICCP chunk in {path}")
            return data[start:end]
        offset = end + (size & 1)
    raise ValueError(f"WebP fixture has no ICCP chunk: {path}")


def encode_png(width: int,
               height: int,
               pixel: PixelFunction,
               icc_profile: bytes | None = None) -> bytes:
    """Encode an unfiltered RGB8 PNG with deterministic chunk ordering."""
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        for x in range(width):
            red, green, blue = pixel(x, y, width, height)
            scanlines.extend((red, green, blue))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    chunks = [png_chunk(b"IHDR", ihdr)]
    if icc_profile is None:
        chunks.append(png_chunk(b"sRGB", b"\x00"))
    else:
        profile = b"A98 RGB\x00\x00" + zlib_stored(icc_profile)
        chunks.append(png_chunk(b"iCCP", profile))
    chunks.append(png_chunk(b"IDAT", zlib_stored(bytes(scanlines))))
    chunks.append(png_chunk(b"IEND", b""))
    return PNG_SIGNATURE + b"".join(chunks)


def smooth_gradient(x: int,
                    y: int,
                    width: int,
                    height: int) -> Tuple[int, int, int]:
    """Sample one resolution-independent two-dimensional RGB ramp."""
    fx = x / max(width - 1, 1)
    fy = y / max(height - 1, 1)
    red = round(255.0 * fx)
    green = round(255.0 * fy)
    blue = round(255.0 * (1.0 - abs(fx - fy)))
    return red, green, blue


def rare_color_pixels(width: int, height: int) -> Dict[Tuple[int, int],
                                                        Tuple[int, int, int]]:
    """Return sparse, individually rare chromatic samples."""
    count = min(8192, width * height // 8)
    state = 0x5EED1234
    pixels: Dict[Tuple[int, int], Tuple[int, int, int]] = {}
    index = 0
    while len(pixels) < count:
        state = (1664525 * state + 1013904223) & 0xFFFFFFFF
        x = state % width
        state = (1664525 * state + 1013904223) & 0xFFFFFFFF
        y = state % height
        if (x, y) in pixels:
            continue
        hue = (index * 0.6180339887498949) % 1.0
        saturation = 0.72 + 0.27 * ((index % 7) / 6.0)
        value = 0.70 + 0.29 * ((index % 11) / 10.0)
        red, green, blue = colorsys.hsv_to_rgb(hue, saturation, value)
        pixels[(x, y)] = (
            round(255.0 * red),
            round(255.0 * green),
            round(255.0 * blue),
        )
        index += 1
    return pixels


def make_rare_color_pixel(width: int, height: int) -> PixelFunction:
    """Build a sparse-color pixel function over a neutral banded field."""
    rare = rare_color_pixels(width, height)

    def pixel(x: int,
              y: int,
              image_width: int,
              image_height: int) -> Tuple[int, int, int]:
        color = rare.get((x, y))
        if color is not None:
            return color
        band = (x * 16) // image_width
        vertical = (y * 4) // image_height
        gray = 24 + band * 13 + vertical * 3
        return gray, gray, gray

    return pixel


def a98_gamut(x: int,
              y: int,
              width: int,
              height: int) -> Tuple[int, int, int]:
    """Sample ramps and saturated primaries in encoded Adobe RGB space."""
    fx = x / max(width - 1, 1)
    fy = y / max(height - 1, 1)
    segment = min(5, (x * 6) // width)
    ramps = (
        (1.0, fy, 0.0),
        (1.0 - fy, 1.0, 0.0),
        (0.0, 1.0, fy),
        (0.0, 1.0 - fy, 1.0),
        (fy, 0.0, 1.0),
        (1.0, 0.0, 1.0 - fy),
    )
    red, green, blue = ramps[segment]
    modulation = 0.65 + 0.35 * (1.0 - abs(2.0 * fx - 1.0))
    return (
        round(255.0 * red * modulation),
        round(255.0 * green * modulation),
        round(255.0 * blue * modulation),
    )


def generated_fixtures(source_root: Path) -> Iterable[Tuple[Path, bytes]]:
    """Yield every generated fixture path and canonical byte representation."""
    directory = source_root / FIXTURE_DIRECTORY
    for width, height in ((128, 96), (600, 450), (900, 675)):
        name = f"smooth-gradient-{width}x{height}.png"
        yield directory / name, encode_png(width, height, smooth_gradient)
    rare = make_rare_color_pixel(600, 450)
    yield directory / "rare-colors-600x450.png", encode_png(600, 450, rare)
    profile = extract_webp_icc(source_root / PROFILE_SOURCE)
    yield (
        directory / "a98-gamut-600x450.png",
        encode_png(600, 450, a98_gamut, profile),
    )


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="compare tracked fixtures with canonical generated bytes",
    )
    return parser.parse_args()


def main() -> int:
    """Write fixtures or verify that checked-in copies are current."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    failed = False
    for path, expected in generated_fixtures(source_root):
        if args.check:
            actual = path.read_bytes() if path.is_file() else b""
            if actual != expected:
                digest = hashlib.sha256(expected).hexdigest()
                print(
                    f"fixture differs: {path} (expected sha256 {digest})",
                    file=sys.stderr,
                )
                failed = True
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(expected)
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
