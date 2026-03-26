#!/usr/bin/env python3
"""Generate 16x16 PSD fixtures for builtin-loader LSQA tests.

This script is only for (re)generating static test assets.
It is not invoked by TAP tests.
"""

import argparse
import math
import pathlib
import struct
import subprocess
import sys
import zlib


WIDTH = 16
HEIGHT = 16


def run_magick_rgb(src_png: pathlib.Path) -> bytes:
    cmd = [
        "magick",
        str(src_png),
        "-resize",
        f"{WIDTH}x{HEIGHT}!",
        "-depth",
        "8",
        "rgb:-",
    ]
    proc = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    expected = WIDTH * HEIGHT * 3
    if len(proc.stdout) != expected:
        raise RuntimeError(
            f"unexpected RGB byte length: got={len(proc.stdout)} expected={expected}"
        )
    return proc.stdout


def split_rgb(rgb_bytes: bytes):
    out = []
    for i in range(0, len(rgb_bytes), 3):
        out.append((rgb_bytes[i], rgb_bytes[i + 1], rgb_bytes[i + 2]))
    return out


def rgb_to_cmyk8(rgb_pixels):
    cmyk = []
    for r, g, b in rgb_pixels:
        if r == 0 and g == 0 and b == 0:
            cmyk.append((0, 0, 0, 255))
            continue
        c = 255 - r
        m = 255 - g
        y = 255 - b
        k = min(c, m, y)
        if k == 255:
            cmyk.append((0, 0, 0, 255))
            continue
        den = 255 - k
        c = ((c - k) * 255) // den
        m = ((m - k) * 255) // den
        y = ((y - k) * 255) // den
        cmyk.append((c, m, y, k))
    return cmyk


def rgb_to_bitmap1(rgb_pixels):
    bits = []
    for r, g, b in rgb_pixels:
        # ITU-R BT.601 luma threshold.
        y = (299 * r + 587 * g + 114 * b) // 1000
        bits.append(1 if y >= 128 else 0)
    return bits


def build_bitmap_plane(bits):
    row_bytes = (WIDTH + 7) // 8
    out = bytearray(row_bytes * HEIGHT)
    for y in range(HEIGHT):
        row_off = y * WIDTH
        dst_off = y * row_bytes
        for x in range(WIDTH):
            if bits[row_off + x]:
                out[dst_off + (x >> 3)] |= 1 << (7 - (x & 7))
    return bytes(out)


def build_alpha8_fade_plane():
    out = bytearray(WIDTH * HEIGHT)
    cx = (WIDTH - 1) / 2.0
    cy = (HEIGHT - 1) / 2.0
    max_d = math.sqrt(cx * cx + cy * cy)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            dx = x - cx
            dy = y - cy
            d = math.sqrt(dx * dx + dy * dy)
            t = 1.0 - min(1.0, d / max_d)
            # Keep soft edges to stress bgcolor compositing around boundaries.
            a = int((t ** 1.7) * 255.0 + 0.5)
            out[y * WIDTH + x] = a
    return bytes(out)


def build_alpha1_plane():
    bits = []
    cx = (WIDTH - 1) / 2.0
    cy = (HEIGHT - 1) / 2.0
    radius = min(WIDTH, HEIGHT) * 0.30
    r2 = radius * radius
    for y in range(HEIGHT):
        for x in range(WIDTH):
            dx = x - cx
            dy = y - cy
            bits.append(1 if (dx * dx + dy * dy) <= r2 else 0)
    return build_bitmap_plane(bits)


def planes_from_cmyk8(cmyk):
    planes = [bytearray(), bytearray(), bytearray(), bytearray()]
    for c, m, y, k in cmyk:
        planes[0].append(c)
        planes[1].append(m)
        planes[2].append(y)
        planes[3].append(k)
    return [bytes(p) for p in planes]


def planes_from_cmyk32(cmyk):
    planes = [bytearray(), bytearray(), bytearray(), bytearray()]
    for c, m, y, k in cmyk:
        planes[0] += struct.pack(">f", c / 255.0)
        planes[1] += struct.pack(">f", m / 255.0)
        planes[2] += struct.pack(">f", y / 255.0)
        planes[3] += struct.pack(">f", k / 255.0)
    return [bytes(p) for p in planes]


def packbits_encode_row(row: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(row)
    while i < n:
        run = 1
        while i + run < n and run < 128 and row[i + run] == row[i]:
            run += 1
        if run >= 3:
            out.append((257 - run) & 0xFF)
            out.append(row[i])
            i += run
            continue
        lit_start = i
        lit_count = 0
        while i < n and lit_count < 128:
            run = 1
            while i + run < n and run < 128 and row[i + run] == row[i]:
                run += 1
            if run >= 3:
                break
            i += 1
            lit_count += 1
        out.append((lit_count - 1) & 0xFF)
        out += row[lit_start : lit_start + lit_count]
    return bytes(out)


def encode_rle(planes, row_bytes):
    row_lengths = []
    row_data = bytearray()
    for plane in planes:
        for y in range(HEIGHT):
            row = plane[y * row_bytes : (y + 1) * row_bytes]
            enc = packbits_encode_row(row)
            if len(enc) > 0xFFFF:
                raise RuntimeError("RLE row too long for PSD")
            row_lengths.append(len(enc))
            row_data += enc
    return row_lengths, bytes(row_data)


def apply_prediction_8bit(plane: bytes, row_bytes: int) -> bytes:
    out = bytearray(plane)
    for y in range(HEIGHT):
        off = y * row_bytes
        prev = out[off]
        for x in range(1, row_bytes):
            cur = out[off + x]
            out[off + x] = (cur - prev) & 0xFF
            prev = cur
    return bytes(out)


def apply_prediction_32bit(plane: bytes, row_bytes: int) -> bytes:
    if row_bytes % 4 != 0:
        raise RuntimeError("row_bytes must be multiple of 4 for 32-bit prediction")
    dwords = row_bytes // 4
    out = bytearray(plane)
    for y in range(HEIGHT):
        off = y * row_bytes
        prev = struct.unpack(">I", out[off : off + 4])[0]
        for i in range(1, dwords):
            pos = off + i * 4
            cur = struct.unpack(">I", out[pos : pos + 4])[0]
            diff = (cur - prev) & 0xFFFFFFFF
            out[pos : pos + 4] = struct.pack(">I", diff)
            prev = cur
    return bytes(out)


def encode_zip(planes, row_bytes, depth, with_prediction):
    payload = bytearray()
    for plane in planes:
        data = plane
        if with_prediction:
            if depth == 8:
                data = apply_prediction_8bit(data, row_bytes)
            elif depth == 32:
                data = apply_prediction_32bit(data, row_bytes)
            else:
                raise RuntimeError("unsupported depth for ZIP prediction")
        payload += data
    return zlib.compress(bytes(payload), level=9)


def build_psd_bytes(planes, *, channels, depth, color_mode, compression):
    if depth == 1:
        row_bytes = (WIDTH + 7) // 8
    elif depth == 8:
        row_bytes = WIDTH
    elif depth == 32:
        row_bytes = WIDTH * 4
    else:
        raise RuntimeError(f"unsupported depth: {depth}")
    plane_bytes = row_bytes * HEIGHT
    if len(planes) != channels:
        raise RuntimeError("plane count mismatch")
    for plane in planes:
        if len(plane) != plane_bytes:
            raise RuntimeError("plane size mismatch")

    out = bytearray()
    out += b"8BPS"
    out += struct.pack(">H", 1)
    out += b"\x00" * 6
    out += struct.pack(">H", channels)
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", depth)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", 0)  # layer/mask info length
    out += struct.pack(">H", compression)

    if compression == 0:
        for plane in planes:
            out += plane
    elif compression == 1:
        row_lengths, row_data = encode_rle(planes, row_bytes)
        for n in row_lengths:
            out += struct.pack(">H", n)
        out += row_data
    elif compression == 2:
        out += encode_zip(planes, row_bytes, depth, with_prediction=False)
    elif compression == 3:
        out += encode_zip(planes, row_bytes, depth, with_prediction=True)
    else:
        raise RuntimeError(f"unsupported compression: {compression}")
    return bytes(out)


def write_file(path: pathlib.Path, data: bytes):
    path.write_bytes(data)
    print(path)


def generate(out_dir: pathlib.Path):
    src_png = out_dir.parent / "snake_64.png"
    rgb = split_rgb(run_magick_rgb(src_png))
    cmyk8 = rgb_to_cmyk8(rgb)
    bitmap = rgb_to_bitmap1(rgb)

    cmyk8_planes = planes_from_cmyk8(cmyk8)
    cmyk32_planes = planes_from_cmyk32(cmyk8)
    bitmap_plane = [build_bitmap_plane(bitmap)]
    alpha8_plane = build_alpha8_fade_plane()
    alpha1_plane = build_alpha1_plane()

    write_file(
        out_dir / "snake16_cmyk8_raw.psd",
        build_psd_bytes(
            cmyk8_planes, channels=4, depth=8, color_mode=4, compression=0
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_rle.psd",
        build_psd_bytes(
            cmyk8_planes, channels=4, depth=8, color_mode=4, compression=1
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_zip.psd",
        build_psd_bytes(
            cmyk8_planes, channels=4, depth=8, color_mode=4, compression=2
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_zip_pred.psd",
        build_psd_bytes(
            cmyk8_planes, channels=4, depth=8, color_mode=4, compression=3
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_alpha.psd",
        build_psd_bytes(
            cmyk8_planes + [alpha8_plane],
            channels=5,
            depth=8,
            color_mode=4,
            compression=0,
        ),
    )

    write_file(
        out_dir / "snake16_bitmap1_raw.psd",
        build_psd_bytes(bitmap_plane, channels=1, depth=1, color_mode=0, compression=0),
    )
    write_file(
        out_dir / "snake16_bitmap1_rle.psd",
        build_psd_bytes(bitmap_plane, channels=1, depth=1, color_mode=0, compression=1),
    )
    write_file(
        out_dir / "snake16_bitmap1_zip.psd",
        build_psd_bytes(bitmap_plane, channels=1, depth=1, color_mode=0, compression=2),
    )
    write_file(
        out_dir / "snake16_bitmap1_alpha.psd",
        build_psd_bytes(
            bitmap_plane + [alpha1_plane], channels=2, depth=1, color_mode=0, compression=0
        ),
    )

    write_file(
        out_dir / "snake16_cmyk32_raw.psd",
        build_psd_bytes(
            cmyk32_planes, channels=4, depth=32, color_mode=4, compression=0
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_zip.psd",
        build_psd_bytes(
            cmyk32_planes, channels=4, depth=32, color_mode=4, compression=2
        ),
    )


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out-dir",
        default=None,
        help="directory for generated PSD files (defaults to script directory)",
    )
    ns = parser.parse_args(argv)
    if ns.out_dir is None:
        out_dir = pathlib.Path(__file__).resolve().parent
    else:
        out_dir = pathlib.Path(ns.out_dir).resolve()
    generate(out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
