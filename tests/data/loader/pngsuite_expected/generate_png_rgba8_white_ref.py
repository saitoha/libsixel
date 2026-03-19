#!/usr/bin/env python3
"""Generate deterministic PNGSuite reference PPM for RGBA8 + white background.

This helper intentionally avoids img2sixel/sixel2png so expected images do not
depend on the command under test.
"""

from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


def decode_srgb_unit(value: float) -> float:
    value = max(0.0, min(1.0, value))
    if value <= 0.04045:
        return value / 12.92
    return ((value + 0.055) / 1.055) ** 2.4


def encode_srgb_unit(value: float) -> float:
    value = max(0.0, min(1.0, value))
    if value <= 0.0031308:
        return value * 12.92
    return 1.055 * (value ** (1.0 / 2.4)) - 0.055


def paeth_predictor(left: int, up: int, up_left: int) -> int:
    p = left + up - up_left
    p_left = abs(p - left)
    p_up = abs(p - up)
    p_up_left = abs(p - up_left)
    if p_left <= p_up and p_left <= p_up_left:
        return left
    if p_up <= p_up_left:
        return up
    return up_left


def read_png_rgba8(path: Path) -> tuple[int, int, bytes]:
    payload = path.read_bytes()
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("input is not a PNG file")

    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    interlace = 0
    idat_chunks: list[bytes] = []

    offset = 8
    while offset + 12 <= len(payload):
        chunk_len = struct.unpack(">I", payload[offset : offset + 4])[0]
        chunk_type = payload[offset + 4 : offset + 8]
        chunk_data = payload[offset + 8 : offset + 8 + chunk_len]
        offset += 12 + chunk_len

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", chunk_data
            )
        elif chunk_type == b"IDAT":
            idat_chunks.append(chunk_data)
        elif chunk_type == b"IEND":
            break

    if not (bit_depth == 8 and color_type == 6 and interlace == 0):
        raise ValueError("only non-interlaced RGBA8 PNG is supported")

    raw = zlib.decompress(b"".join(idat_chunks))
    bytes_per_pixel = 4
    stride = width * bytes_per_pixel
    out = bytearray(height * stride)

    pos = 0
    for y in range(height):
        filter_type = raw[pos]
        pos += 1
        row = raw[pos : pos + stride]
        pos += stride

        for x in range(stride):
            left = out[y * stride + x - bytes_per_pixel] if x >= bytes_per_pixel else 0
            up = out[(y - 1) * stride + x] if y > 0 else 0
            up_left = (
                out[(y - 1) * stride + x - bytes_per_pixel]
                if y > 0 and x >= bytes_per_pixel
                else 0
            )
            value = row[x]

            if filter_type == 0:
                restored = value
            elif filter_type == 1:
                restored = (value + left) & 0xFF
            elif filter_type == 2:
                restored = (value + up) & 0xFF
            elif filter_type == 3:
                restored = (value + ((left + up) >> 1)) & 0xFF
            elif filter_type == 4:
                restored = (value + paeth_predictor(left, up, up_left)) & 0xFF
            else:
                raise ValueError(f"unsupported PNG filter: {filter_type}")

            out[y * stride + x] = restored

    return width, height, bytes(out)


def compose_white_background_to_ppm(src_png: Path, dst_ppm: Path) -> None:
    width, height, rgba = read_png_rgba8(src_png)
    dst_values: list[int] = []

    for i in range(0, len(rgba), 4):
        red = rgba[i]
        green = rgba[i + 1]
        blue = rgba[i + 2]
        alpha = rgba[i + 3] / 255.0

        red_linear = decode_srgb_unit(red / 255.0)
        green_linear = decode_srgb_unit(green / 255.0)
        blue_linear = decode_srgb_unit(blue / 255.0)

        out_red = encode_srgb_unit(red_linear * alpha + (1.0 - alpha))
        out_green = encode_srgb_unit(green_linear * alpha + (1.0 - alpha))
        out_blue = encode_srgb_unit(blue_linear * alpha + (1.0 - alpha))

        dst_values.extend(
            (
                int(max(0, min(255, out_red * 255.0 + 0.5))),
                int(max(0, min(255, out_green * 255.0 + 0.5))),
                int(max(0, min(255, out_blue * 255.0 + 0.5))),
            )
        )

    with dst_ppm.open("w", encoding="ascii") as f:
        f.write(f"P3\n{width} {height}\n255\n")
        for index, value in enumerate(dst_values):
            f.write(str(value))
            if (index + 1) % 12 == 0:
                f.write("\n")
            else:
                f.write(" ")
        if len(dst_values) % 12 != 0:
            f.write("\n")


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <input_png> <output_ppm>", file=sys.stderr)
        return 1

    input_png = Path(sys.argv[1])
    output_ppm = Path(sys.argv[2])
    compose_white_background_to_ppm(input_png, output_ppm)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
