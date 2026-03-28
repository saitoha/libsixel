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


def rgb_to_gray8(rgb_pixels):
    out = []
    for r, g, b in rgb_pixels:
        # ITU-R BT.601 luma approximation in 8-bit integer space.
        y = (299 * r + 587 * g + 114 * b) // 1000
        out.append(y)
    return out


def srgb_u8_to_linear(v: int) -> float:
    c = float(v) / 255.0
    if c <= 0.04045:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def f_lab(t: float) -> float:
    delta = 6.0 / 29.0
    if t > delta ** 3:
        return t ** (1.0 / 3.0)
    return t / (3.0 * delta * delta) + 4.0 / 29.0


def rgb_to_lab(rgb_pixels):
    out = []
    for r, g, b in rgb_pixels:
        rl = srgb_u8_to_linear(r)
        gl = srgb_u8_to_linear(g)
        bl = srgb_u8_to_linear(b)

        # D65-adapted sRGB -> XYZ (0..1 scale)
        x = rl * 0.4124564 + gl * 0.3575761 + bl * 0.1804375
        y = rl * 0.2126729 + gl * 0.7151522 + bl * 0.0721750
        z = rl * 0.0193339 + gl * 0.1191920 + bl * 0.9503041

        # D65 reference white
        fx = f_lab(x / 0.95047)
        fy = f_lab(y / 1.00000)
        fz = f_lab(z / 1.08883)

        l = 116.0 * fy - 16.0
        a = 500.0 * (fx - fy)
        bstar = 200.0 * (fy - fz)
        out.append((l, a, bstar))
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


def expand_u8_plane_to_u16be(plane_u8: bytes) -> bytes:
    out = bytearray()
    for v in plane_u8:
        out += struct.pack(">H", v * 257)
    return bytes(out)


def planes_from_rgb8(rgb_pixels):
    planes = [bytearray(), bytearray(), bytearray()]
    for r, g, b in rgb_pixels:
        planes[0].append(r)
        planes[1].append(g)
        planes[2].append(b)
    return [bytes(p) for p in planes]


def planes_from_rgb16(rgb_pixels):
    planes = [bytearray(), bytearray(), bytearray()]
    for r, g, b in rgb_pixels:
        planes[0] += struct.pack(">H", r * 257)
        planes[1] += struct.pack(">H", g * 257)
        planes[2] += struct.pack(">H", b * 257)
    return [bytes(p) for p in planes]


def planes_from_rgb32(rgb_pixels):
    planes = [bytearray(), bytearray(), bytearray()]
    for r, g, b in rgb_pixels:
        planes[0] += struct.pack(">f", r / 255.0)
        planes[1] += struct.pack(">f", g / 255.0)
        planes[2] += struct.pack(">f", b / 255.0)
    return [bytes(p) for p in planes]


def planes_from_gray8(gray_values):
    return [bytes(gray_values)]


def planes_from_gray16(gray_values):
    out = bytearray()
    for v in gray_values:
        out += struct.pack(">H", v * 257)
    return [bytes(out)]


def planes_from_gray32(gray_values):
    out = bytearray()
    for v in gray_values:
        out += struct.pack(">f", v / 255.0)
    return [bytes(out)]


def planes_from_lab8(lab_values):
    planes = [bytearray(), bytearray(), bytearray()]
    for l, a, bstar in lab_values:
        l8 = int(max(0.0, min(255.0, l * 255.0 / 100.0)) + 0.5)
        a8 = int(max(0.0, min(255.0, a + 128.0)) + 0.5)
        b8 = int(max(0.0, min(255.0, bstar + 128.0)) + 0.5)
        planes[0].append(l8)
        planes[1].append(a8)
        planes[2].append(b8)
    return [bytes(p) for p in planes]


def planes_from_lab32(lab_values):
    planes = [bytearray(), bytearray(), bytearray()]
    for l, a, bstar in lab_values:
        planes[0] += struct.pack(">f", l)
        planes[1] += struct.pack(">f", a)
        planes[2] += struct.pack(">f", bstar)
    return [bytes(p) for p in planes]


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


def build_indexed_mode(rgb_pixels):
    # 16x16 has exactly 256 pixels, so we assign one palette index per pixel
    # and preserve the resized snake image exactly in indexed form.
    indices = bytearray()
    pal_r = bytearray()
    pal_g = bytearray()
    pal_b = bytearray()
    for i, (r, g, b) in enumerate(rgb_pixels):
        idx = i & 0xFF
        indices.append(idx)
        pal_r.append(r)
        pal_g.append(g)
        pal_b.append(b)
    color_mode_data = bytes(pal_r + pal_g + pal_b)
    if len(color_mode_data) != 768:
        raise RuntimeError("indexed color mode data must be 768 bytes")
    return [bytes(indices)], color_mode_data


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


def apply_prediction_16bit(plane: bytes, row_bytes: int) -> bytes:
    if row_bytes % 2 != 0:
        raise RuntimeError("row_bytes must be multiple of 2 for 16-bit prediction")
    words = row_bytes // 2
    out = bytearray(plane)
    for y in range(HEIGHT):
        off = y * row_bytes
        prev = struct.unpack(">H", out[off : off + 2])[0]
        for i in range(1, words):
            pos = off + i * 2
            cur = struct.unpack(">H", out[pos : pos + 2])[0]
            diff = (cur - prev) & 0xFFFF
            out[pos : pos + 2] = struct.pack(">H", diff)
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
            elif depth == 16:
                data = apply_prediction_16bit(data, row_bytes)
            elif depth == 32:
                data = apply_prediction_32bit(data, row_bytes)
            else:
                raise RuntimeError("unsupported depth for ZIP prediction")
        payload += data
    return zlib.compress(bytes(payload), level=9)


def build_psd_bytes(
    planes,
    *,
    channels,
    depth,
    color_mode,
    compression,
    color_mode_data=b"",
):
    if depth == 1:
        row_bytes = (WIDTH + 7) // 8
    elif depth == 8:
        row_bytes = WIDTH
    elif depth == 16:
        row_bytes = WIDTH * 2
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
    out += struct.pack(">I", len(color_mode_data))
    out += color_mode_data
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


def build_psd_layer_only_single_rgb8(planes):
    if len(planes) < 3:
        raise RuntimeError("layer-only RGB fixture requires R/G/B planes")
    row_bytes = WIDTH
    plane_bytes = row_bytes * HEIGHT
    for i in range(3):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected RGB8 plane size")

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", 3)
    for channel_id, plane in enumerate(planes[:3]):
        payload = struct.pack(">H", 0) + plane
        layer_record += struct.pack(">hI", channel_id, len(payload))
        channel_payload += payload
    layer_record += b"8BIMnorm"
    layer_record += bytes([255, 0, 0, 0])
    layer_record += struct.pack(">I", 0)

    layer_info = bytearray()
    layer_info += struct.pack(">h", 1)
    layer_info += layer_record
    layer_info += channel_payload

    layer_and_mask = struct.pack(">I", len(layer_info)) + layer_info

    out = bytearray()
    out += b"8BPS"
    out += struct.pack(">H", 1)
    out += b"\x00" * 6
    out += struct.pack(">H", 3)
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 8)
    out += struct.pack(">H", 3)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def write_file(path: pathlib.Path, data: bytes):
    path.write_bytes(data)
    print(path)


def write_variants(
    out_dir: pathlib.Path,
    base_name: str,
    planes,
    *,
    channels: int,
    depth: int,
    color_mode: int,
    variants,
    color_mode_data: bytes = b"",
):
    compression_by_name = {
        "raw": 0,
        "rle": 1,
        "zip": 2,
        "zip_pred": 3,
    }
    for variant in variants:
        compression = compression_by_name[variant]
        write_file(
            out_dir / f"{base_name}_{variant}.psd",
            build_psd_bytes(
                planes,
                channels=channels,
                depth=depth,
                color_mode=color_mode,
                compression=compression,
                color_mode_data=color_mode_data,
            ),
        )


def generate(out_dir: pathlib.Path):
    src_png = out_dir.parent / "snake_64.png"
    rgb = split_rgb(run_magick_rgb(src_png))
    gray8 = rgb_to_gray8(rgb)
    lab = rgb_to_lab(rgb)
    cmyk8 = rgb_to_cmyk8(rgb)
    bitmap = rgb_to_bitmap1(rgb)

    rgb8_planes = planes_from_rgb8(rgb)
    rgb16_planes = planes_from_rgb16(rgb)
    rgb32_planes = planes_from_rgb32(rgb)

    gray8_plane = planes_from_gray8(gray8)[0]
    gray16_plane = planes_from_gray16(gray8)[0]
    gray32_plane = planes_from_gray32(gray8)[0]

    lab8_planes = planes_from_lab8(lab)
    # Keep Lab32 chroma small so builtin ZIP+prediction decode and
    # coregraphics raw decode stay comparable in cross-loader LSQA.
    lab32_chroma_scale = 0.02
    lab32_values = [(l, a * lab32_chroma_scale, b * lab32_chroma_scale)
                    for (l, a, b) in lab]
    lab32_planes = planes_from_lab32(lab32_values)

    cmyk8_planes = planes_from_cmyk8(cmyk8)
    cmyk32_planes = planes_from_cmyk32(cmyk8)

    bitmap_plane = [build_bitmap_plane(bitmap)]

    alpha8_plane = build_alpha8_fade_plane()
    alpha16_plane = expand_u8_plane_to_u16be(alpha8_plane)
    alpha1_plane = build_alpha1_plane()

    indexed_planes, indexed_color_mode_data = build_indexed_mode(rgb)

    # RGB / Gray / Duotone / Indexed / Lab 8-bit fixtures
    write_variants(
        out_dir,
        "snake16_rgb8",
        rgb8_planes,
        channels=3,
        depth=8,
        color_mode=3,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_rgb8_alpha.psd",
        build_psd_bytes(
            rgb8_planes + [alpha8_plane],
            channels=4,
            depth=8,
            color_mode=3,
            compression=0,
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb8(rgb8_planes),
    )

    write_variants(
        out_dir,
        "snake16_gray8",
        [gray8_plane],
        channels=1,
        depth=8,
        color_mode=1,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_gray8_alpha.psd",
        build_psd_bytes(
            [gray8_plane, alpha8_plane],
            channels=2,
            depth=8,
            color_mode=1,
            compression=0,
        ),
    )

    write_variants(
        out_dir,
        "snake16_duotone8",
        [gray8_plane],
        channels=1,
        depth=8,
        color_mode=8,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_duotone8_alpha.psd",
        build_psd_bytes(
            [gray8_plane, alpha8_plane],
            channels=2,
            depth=8,
            color_mode=8,
            compression=0,
        ),
    )

    write_variants(
        out_dir,
        "snake16_indexed8",
        indexed_planes,
        channels=1,
        depth=8,
        color_mode=2,
        variants=["raw", "rle", "zip", "zip_pred"],
        color_mode_data=indexed_color_mode_data,
    )
    write_file(
        out_dir / "snake16_indexed8_alpha.psd",
        build_psd_bytes(
            indexed_planes + [alpha8_plane],
            channels=2,
            depth=8,
            color_mode=2,
            compression=0,
            color_mode_data=indexed_color_mode_data,
        ),
    )

    write_variants(
        out_dir,
        "snake16_lab8",
        lab8_planes,
        channels=3,
        depth=8,
        color_mode=9,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_lab8_alpha.psd",
        build_psd_bytes(
            lab8_planes + [alpha8_plane],
            channels=4,
            depth=8,
            color_mode=9,
            compression=0,
        ),
    )

    # 16-bit fixtures
    write_variants(
        out_dir,
        "snake16_rgb16",
        rgb16_planes,
        channels=3,
        depth=16,
        color_mode=3,
        variants=["raw", "rle", "zip", "zip_pred"],
    )

    write_variants(
        out_dir,
        "snake16_gray16",
        [gray16_plane],
        channels=1,
        depth=16,
        color_mode=1,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_gray16_alpha.psd",
        build_psd_bytes(
            [gray16_plane, alpha16_plane],
            channels=2,
            depth=16,
            color_mode=1,
            compression=0,
        ),
    )

    write_variants(
        out_dir,
        "snake16_duotone16",
        [gray16_plane],
        channels=1,
        depth=16,
        color_mode=8,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_duotone16_alpha.psd",
        build_psd_bytes(
            [gray16_plane, alpha16_plane],
            channels=2,
            depth=16,
            color_mode=8,
            compression=0,
        ),
    )

    # 32-bit fixtures
    write_variants(
        out_dir,
        "snake16_rgb32",
        rgb32_planes,
        channels=3,
        depth=32,
        color_mode=3,
        variants=["raw", "zip", "zip_pred"],
    )
    write_variants(
        out_dir,
        "snake16_gray32",
        [gray32_plane],
        channels=1,
        depth=32,
        color_mode=1,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_variants(
        out_dir,
        "snake16_lab32",
        lab32_planes,
        channels=3,
        depth=32,
        color_mode=9,
        variants=["raw", "rle", "zip", "zip_pred"],
    )

    # Existing CMYK and Bitmap fixtures (kept and expanded from same source)
    write_variants(
        out_dir,
        "snake16_cmyk8",
        cmyk8_planes,
        channels=4,
        depth=8,
        color_mode=4,
        variants=["raw", "rle", "zip", "zip_pred"],
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

    write_variants(
        out_dir,
        "snake16_cmyk32",
        cmyk32_planes,
        channels=4,
        depth=32,
        color_mode=4,
        variants=["raw", "rle", "zip", "zip_pred"],
    )

    write_variants(
        out_dir,
        "snake16_bitmap1",
        bitmap_plane,
        channels=1,
        depth=1,
        color_mode=0,
        variants=["raw", "rle", "zip"],
    )
    write_file(
        out_dir / "snake16_bitmap1_alpha.psd",
        build_psd_bytes(
            bitmap_plane + [alpha1_plane],
            channels=2,
            depth=1,
            color_mode=0,
            compression=0,
        ),
    )

    # Stress fixture: RGB with 16 channels.
    # The first extra channel is forced opaque so boundary-policy tests stay
    # stable even if decoders treat it as transparency.
    extra_channel = gray8_plane
    opaque_alpha_plane = b"\xff" * len(gray8_plane)
    channels16_planes = rgb8_planes + [opaque_alpha_plane] + [extra_channel] * 12
    write_file(
        out_dir / "snake16_channels16_rgb.psd",
        build_psd_bytes(
            channels16_planes,
            channels=16,
            depth=8,
            color_mode=3,
            compression=0,
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
