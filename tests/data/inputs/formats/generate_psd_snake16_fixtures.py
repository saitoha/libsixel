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


def build_sxfl_soco_payload(r: int, g: int, b: int) -> bytes:
    return bytes(
        [
            ord("S"),
            ord("X"),
            ord("F"),
            ord("L"),
            1,  # version
            1,  # kind SoCo
            0,
            0,
            r & 0xFF,
            g & 0xFF,
            b & 0xFF,
        ]
    )


def _descriptor_unicode(text: str) -> bytes:
    encoded = text.encode("utf-16be")
    return struct.pack(">I", len(text)) + encoded


def _descriptor_key4(key4: bytes) -> bytes:
    if len(key4) != 4:
        raise ValueError(f"descriptor key must be 4 bytes, got {key4!r}")
    return struct.pack(">I", 0) + key4


def build_descriptor_soco_payload(r: int, g: int, b: int) -> bytes:
    r = int(max(0, min(255, r)))
    g = int(max(0, min(255, g)))
    b = int(max(0, min(255, b)))

    color_object = bytearray()
    color_object += _descriptor_unicode("")
    color_object += _descriptor_key4(b"RGBC")
    color_object += struct.pack(">I", 3)
    color_object += _descriptor_key4(b"Rd  ")
    color_object += b"doub" + struct.pack(">d", float(r))
    color_object += _descriptor_key4(b"Grn ")
    color_object += b"doub" + struct.pack(">d", float(g))
    color_object += _descriptor_key4(b"Bl  ")
    color_object += b"doub" + struct.pack(">d", float(b))

    root = bytearray()
    root += _descriptor_unicode("")
    root += _descriptor_key4(b"SoCo")
    root += struct.pack(">I", 1)
    root += _descriptor_key4(b"Clr ")
    root += b"Objc"
    root += color_object
    return bytes(root)


def build_descriptor_soco_payload_cmyk(c: float, m: float, y: float, k: float) -> bytes:
    c = float(max(0.0, min(100.0, c)))
    m = float(max(0.0, min(100.0, m)))
    y = float(max(0.0, min(100.0, y)))
    k = float(max(0.0, min(100.0, k)))

    color_object = bytearray()
    color_object += _descriptor_unicode("")
    color_object += _descriptor_key4(b"CMYC")
    color_object += struct.pack(">I", 4)
    color_object += _descriptor_key4(b"Cyn ")
    color_object += b"doub" + struct.pack(">d", c)
    color_object += _descriptor_key4(b"Mgnt")
    color_object += b"doub" + struct.pack(">d", m)
    color_object += _descriptor_key4(b"Ylw ")
    color_object += b"doub" + struct.pack(">d", y)
    color_object += _descriptor_key4(b"Blck")
    color_object += b"doub" + struct.pack(">d", k)

    root = bytearray()
    root += _descriptor_unicode("")
    root += _descriptor_key4(b"SoCo")
    root += struct.pack(">I", 1)
    root += _descriptor_key4(b"Clr ")
    root += b"Objc"
    root += color_object
    return bytes(root)


def build_descriptor_soco_payload_gray(gray: float) -> bytes:
    gray = float(max(0.0, min(100.0, gray)))

    color_object = bytearray()
    color_object += _descriptor_unicode("")
    color_object += _descriptor_key4(b"Grsc")
    color_object += struct.pack(">I", 1)
    color_object += _descriptor_key4(b"Gry ")
    color_object += b"doub" + struct.pack(">d", gray)

    root = bytearray()
    root += _descriptor_unicode("")
    root += _descriptor_key4(b"SoCo")
    root += struct.pack(">I", 1)
    root += _descriptor_key4(b"Clr ")
    root += b"Objc"
    root += color_object
    return bytes(root)


def build_descriptor_soco_payload_hsb(hue_deg: float, saturation_pct: float, brightness_pct: float) -> bytes:
    hue_deg = float(hue_deg)
    saturation_pct = float(max(0.0, min(100.0, saturation_pct)))
    brightness_pct = float(max(0.0, min(100.0, brightness_pct)))

    color_object = bytearray()
    color_object += _descriptor_unicode("")
    color_object += _descriptor_key4(b"HSBC")
    color_object += struct.pack(">I", 3)
    color_object += _descriptor_key4(b"H   ")
    color_object += b"doub" + struct.pack(">d", hue_deg)
    color_object += _descriptor_key4(b"Strt")
    color_object += b"doub" + struct.pack(">d", saturation_pct)
    color_object += _descriptor_key4(b"Brgh")
    color_object += b"doub" + struct.pack(">d", brightness_pct)

    root = bytearray()
    root += _descriptor_unicode("")
    root += _descriptor_key4(b"SoCo")
    root += struct.pack(">I", 1)
    root += _descriptor_key4(b"Clr ")
    root += b"Objc"
    root += color_object
    return bytes(root)


def build_descriptor_soco_payload_lab(l_star: float, a_star: float, b_star: float) -> bytes:
    l_star = float(max(0.0, min(100.0, l_star)))
    a_star = float(max(-128.0, min(127.0, a_star)))
    b_star = float(max(-128.0, min(127.0, b_star)))

    color_object = bytearray()
    color_object += _descriptor_unicode("")
    color_object += _descriptor_key4(b"LbCl")
    color_object += struct.pack(">I", 3)
    color_object += _descriptor_key4(b"Lmnc")
    color_object += b"doub" + struct.pack(">d", l_star)
    color_object += _descriptor_key4(b"A   ")
    color_object += b"doub" + struct.pack(">d", a_star)
    color_object += _descriptor_key4(b"B   ")
    color_object += b"doub" + struct.pack(">d", b_star)

    root = bytearray()
    root += _descriptor_unicode("")
    root += _descriptor_key4(b"SoCo")
    root += struct.pack(">I", 1)
    root += _descriptor_key4(b"Clr ")
    root += b"Objc"
    root += color_object
    return bytes(root)


def build_descriptor_gdfl_payload(
    *,
    gradient_type_key: bytes,
    reverse: bool,
    angle_deg: float,
    scale_percent: float,
    stops,
    use_nested_grad: bool = False,
) -> bytes:
    def build_rgb_object(r: int, g: int, b: int) -> bytes:
        obj = bytearray()
        obj += _descriptor_unicode("")
        obj += _descriptor_key4(b"RGBC")
        obj += struct.pack(">I", 3)
        obj += _descriptor_key4(b"Rd  ")
        obj += b"doub" + struct.pack(">d", float(max(0, min(255, int(r)))))
        obj += _descriptor_key4(b"Grn ")
        obj += b"doub" + struct.pack(">d", float(max(0, min(255, int(g)))))
        obj += _descriptor_key4(b"Bl  ")
        obj += b"doub" + struct.pack(">d", float(max(0, min(255, int(b)))))
        return bytes(obj)

    def build_lab_object(l_star: float, a_star: float, b_star: float) -> bytes:
        obj = bytearray()
        obj += _descriptor_unicode("")
        obj += _descriptor_key4(b"LbCl")
        obj += struct.pack(">I", 3)
        obj += _descriptor_key4(b"Lmnc")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, l_star))))
        obj += _descriptor_key4(b"A   ")
        obj += b"doub" + struct.pack(">d", float(max(-128.0, min(127.0, a_star))))
        obj += _descriptor_key4(b"B   ")
        obj += b"doub" + struct.pack(">d", float(max(-128.0, min(127.0, b_star))))
        return bytes(obj)

    def build_cmyk_object(cyan: float, magenta: float, yellow: float, black: float) -> bytes:
        obj = bytearray()
        obj += _descriptor_unicode("")
        obj += _descriptor_key4(b"CMYC")
        obj += struct.pack(">I", 4)
        obj += _descriptor_key4(b"Cyn ")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, cyan))))
        obj += _descriptor_key4(b"Mgnt")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, magenta))))
        obj += _descriptor_key4(b"Ylw ")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, yellow))))
        obj += _descriptor_key4(b"Blck")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, black))))
        return bytes(obj)

    def build_gray_object(gray: float) -> bytes:
        obj = bytearray()
        obj += _descriptor_unicode("")
        obj += _descriptor_key4(b"Grsc")
        obj += struct.pack(">I", 1)
        obj += _descriptor_key4(b"Gry ")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, gray))))
        return bytes(obj)

    def build_hsb_object(hue_deg: float, saturation_pct: float, brightness_pct: float) -> bytes:
        obj = bytearray()
        obj += _descriptor_unicode("")
        obj += _descriptor_key4(b"HSBC")
        obj += struct.pack(">I", 3)
        obj += _descriptor_key4(b"H   ")
        obj += b"doub" + struct.pack(">d", float(hue_deg))
        obj += _descriptor_key4(b"Strt")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, saturation_pct))))
        obj += _descriptor_key4(b"Brgh")
        obj += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, brightness_pct))))
        return bytes(obj)

    def build_stop_object(pos: float, color_object: bytes, opacity_percent: float) -> bytes:
        loc = int(max(0.0, min(1.0, pos)) * 4096.0 + 0.5)
        opct = float(max(0.0, min(100.0, opacity_percent)))
        obj = bytearray()
        obj += _descriptor_unicode("")
        obj += _descriptor_key4(b"Clrt")
        obj += struct.pack(">I", 3)
        obj += _descriptor_key4(b"Lctn")
        obj += b"long" + struct.pack(">I", loc)
        obj += _descriptor_key4(b"Clr ")
        obj += b"Objc" + color_object
        obj += _descriptor_key4(b"Opct")
        obj += b"UntF" + b"#Prc" + struct.pack(">d", opct)
        return bytes(obj)

    if len(gradient_type_key) != 4:
        raise ValueError(f"gradient_type_key must be 4 bytes, got {gradient_type_key!r}")

    clrs = bytearray()
    clrs += struct.pack(">I", len(stops))
    for stop in stops:
        if len(stop) == 5:
            pos, r, g, b, opacity = stop
            color_object = build_rgb_object(r, g, b)
        elif len(stop) == 3:
            pos, color_spec, opacity = stop
            if isinstance(color_spec, (bytes, bytearray)):
                color_object = bytes(color_spec)
            elif isinstance(color_spec, tuple) and len(color_spec) == 4 and color_spec[0] == "lab":
                color_object = build_lab_object(color_spec[1], color_spec[2], color_spec[3])
            elif isinstance(color_spec, tuple) and len(color_spec) == 5 and color_spec[0] == "cmyk":
                color_object = build_cmyk_object(color_spec[1],
                                                 color_spec[2],
                                                 color_spec[3],
                                                 color_spec[4])
            elif isinstance(color_spec, tuple) and len(color_spec) == 2 and color_spec[0] == "gray":
                color_object = build_gray_object(color_spec[1])
            elif isinstance(color_spec, tuple) and len(color_spec) == 4 and color_spec[0] == "hsb":
                color_object = build_hsb_object(color_spec[1],
                                                color_spec[2],
                                                color_spec[3])
            else:
                raise ValueError(f"unsupported gradient stop color spec: {color_spec!r}")
        else:
            raise ValueError(f"unsupported gradient stop shape: {stop!r}")
        clrs += b"Objc"
        clrs += build_stop_object(pos, color_object, opacity)

    root = bytearray()
    root += _descriptor_unicode("")
    root += _descriptor_key4(b"GrFl")
    if use_nested_grad:
        grad = bytearray()
        grad += _descriptor_unicode("")
        grad += _descriptor_key4(b"Grdn")
        grad += struct.pack(">I", 2)
        grad += _descriptor_key4(b"Type")
        grad += b"enum" + _descriptor_key4(b"GrdT") + _descriptor_key4(gradient_type_key)
        grad += _descriptor_key4(b"Clrs")
        grad += b"VlLs" + clrs

        root += struct.pack(">I", 4)
        root += _descriptor_key4(b"Rvrs")
        root += b"bool" + (b"\x01" if reverse else b"\x00")
        root += _descriptor_key4(b"Angl")
        root += b"UntF" + b"#Ang" + struct.pack(">d", float(angle_deg))
        root += _descriptor_key4(b"Scl ")
        root += b"UntF" + b"#Prc" + struct.pack(">d", float(scale_percent))
        root += _descriptor_key4(b"Grad")
        root += b"Objc" + grad
    else:
        root += struct.pack(">I", 5)
        root += _descriptor_key4(b"Type")
        root += b"enum" + _descriptor_key4(b"GrdT") + _descriptor_key4(gradient_type_key)
        root += _descriptor_key4(b"Rvrs")
        root += b"bool" + (b"\x01" if reverse else b"\x00")
        root += _descriptor_key4(b"Angl")
        root += b"UntF" + b"#Ang" + struct.pack(">d", float(angle_deg))
        root += _descriptor_key4(b"Scl ")
        root += b"UntF" + b"#Prc" + struct.pack(">d", float(scale_percent))
        root += _descriptor_key4(b"Clrs")
        root += b"VlLs" + clrs
    return bytes(root)


def build_sxfl_gdfl_payload(
    *,
    gradient_type: int,
    reverse: bool,
    angle_deg: float,
    scale: float,
    stops,
) -> bytes:
    out = bytearray()
    out += b"SXFL"
    out += bytes([1, 2])  # version=1, kind=GdFl
    out += bytes([gradient_type & 0xFF, 1 if reverse else 0])
    angle_i16 = int(max(-3276.8, min(3276.7, angle_deg)) * 10.0)
    out += struct.pack(">h", angle_i16)
    scale_u16 = int(max(0.01, min(655.35, scale)) * 100.0 + 0.5)
    out += struct.pack(">H", scale_u16)
    out += bytes([min(255, len(stops))])
    for pos, r, g, b, a in stops[:255]:
        p = int(max(0.0, min(1.0, pos)) * 65535.0 + 0.5)
        out += struct.pack(">H", p)
        out += bytes(
            [
                int(max(0, min(255, r))),
                int(max(0, min(255, g))),
                int(max(0, min(255, b))),
                int(max(0, min(255, a))),
            ]
        )
    return bytes(out)


def build_sxfl_ptfl_payload(
    *,
    tile: int,
    fg_rgb,
    bg_rgb,
) -> bytes:
    return bytes(
        [
            ord("S"),
            ord("X"),
            ord("F"),
            ord("L"),
            1,  # version
            3,  # kind PtFl
            max(1, min(255, int(tile))),
            int(max(0, min(255, fg_rgb[0]))),
            int(max(0, min(255, fg_rgb[1]))),
            int(max(0, min(255, fg_rgb[2]))),
            int(max(0, min(255, bg_rgb[0]))),
            int(max(0, min(255, bg_rgb[1]))),
            int(max(0, min(255, bg_rgb[2]))),
        ]
    )


def build_descriptor_ptfl_payload(
    *,
    tile: int,
    fg_rgb,
    bg_rgb,
) -> bytes:
    def build_rgb_object(r: int, g: int, b: int) -> bytes:
        r = int(max(0, min(255, r)))
        g = int(max(0, min(255, g)))
        b = int(max(0, min(255, b)))
        out = bytearray()
        out += _descriptor_unicode("")
        out += _descriptor_key4(b"RGBC")
        out += struct.pack(">I", 3)
        out += _descriptor_key4(b"Rd  ")
        out += b"doub" + struct.pack(">d", float(r))
        out += _descriptor_key4(b"Grn ")
        out += b"doub" + struct.pack(">d", float(g))
        out += _descriptor_key4(b"Bl  ")
        out += b"doub" + struct.pack(">d", float(b))
        return bytes(out)

    def build_cmyk_object(cyan: float, magenta: float, yellow: float, black: float) -> bytes:
        out = bytearray()
        out += _descriptor_unicode("")
        out += _descriptor_key4(b"CMYC")
        out += struct.pack(">I", 4)
        out += _descriptor_key4(b"Cyn ")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, cyan))))
        out += _descriptor_key4(b"Mgnt")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, magenta))))
        out += _descriptor_key4(b"Ylw ")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, yellow))))
        out += _descriptor_key4(b"Blck")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, black))))
        return bytes(out)

    def build_gray_object(gray: float) -> bytes:
        out = bytearray()
        out += _descriptor_unicode("")
        out += _descriptor_key4(b"Grsc")
        out += struct.pack(">I", 1)
        out += _descriptor_key4(b"Gry ")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, gray))))
        return bytes(out)

    def build_hsb_object(hue_deg: float, saturation_pct: float, brightness_pct: float) -> bytes:
        out = bytearray()
        out += _descriptor_unicode("")
        out += _descriptor_key4(b"HSBC")
        out += struct.pack(">I", 3)
        out += _descriptor_key4(b"H   ")
        out += b"doub" + struct.pack(">d", float(hue_deg))
        out += _descriptor_key4(b"Strt")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, saturation_pct))))
        out += _descriptor_key4(b"Brgh")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, brightness_pct))))
        return bytes(out)

    def build_lab_object(l_star: float, a_star: float, b_star: float) -> bytes:
        out = bytearray()
        out += _descriptor_unicode("")
        out += _descriptor_key4(b"LbCl")
        out += struct.pack(">I", 3)
        out += _descriptor_key4(b"Lmnc")
        out += b"doub" + struct.pack(">d", float(max(0.0, min(100.0, l_star))))
        out += _descriptor_key4(b"A   ")
        out += b"doub" + struct.pack(">d", float(max(-128.0, min(127.0, a_star))))
        out += _descriptor_key4(b"B   ")
        out += b"doub" + struct.pack(">d", float(max(-128.0, min(127.0, b_star))))
        return bytes(out)

    def build_color_object(color_spec) -> bytes:
        if (
            isinstance(color_spec, (tuple, list))
            and len(color_spec) == 3
            and not isinstance(color_spec[0], str)
        ):
            return build_rgb_object(color_spec[0], color_spec[1], color_spec[2])
        if (
            isinstance(color_spec, tuple)
            and len(color_spec) == 5
            and color_spec[0] == "cmyk"
        ):
            return build_cmyk_object(color_spec[1],
                                     color_spec[2],
                                     color_spec[3],
                                     color_spec[4])
        if (
            isinstance(color_spec, tuple)
            and len(color_spec) == 2
            and color_spec[0] == "gray"
        ):
            return build_gray_object(color_spec[1])
        if (
            isinstance(color_spec, tuple)
            and len(color_spec) == 4
            and color_spec[0] == "hsb"
        ):
            return build_hsb_object(color_spec[1],
                                    color_spec[2],
                                    color_spec[3])
        if (
            isinstance(color_spec, tuple)
            and len(color_spec) == 4
            and color_spec[0] == "lab"
        ):
            return build_lab_object(color_spec[1],
                                    color_spec[2],
                                    color_spec[3])
        raise ValueError(f"unsupported PtFl descriptor color spec: {color_spec!r}")

    tile_i = int(max(1, min(65535, tile)))
    out = bytearray()
    out += _descriptor_unicode("")
    out += _descriptor_key4(b"PtFl")
    out += struct.pack(">I", 3)
    out += _descriptor_key4(b"Sz  ")
    out += b"long" + struct.pack(">I", tile_i)
    out += _descriptor_key4(b"FgCl")
    out += b"Objc" + build_color_object(fg_rgb)
    out += _descriptor_key4(b"BgCl")
    out += b"Objc" + build_color_object(bg_rgb)
    return bytes(out)


def expand_u8_plane_to_u16be(plane_u8: bytes) -> bytes:
    out = bytearray()
    for v in plane_u8:
        out += struct.pack(">H", v * 257)
    return bytes(out)


def expand_u8_plane_to_f32be(plane_u8: bytes) -> bytes:
    out = bytearray()
    for v in plane_u8:
        out += struct.pack(">f", v / 255.0)
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


def planes_from_lab16(lab_values):
    planes = [bytearray(), bytearray(), bytearray()]
    for l, a, bstar in lab_values:
        l16 = int(max(0.0, min(65535.0, (l / 100.0) * 65535.0)) + 0.5)
        a16 = int(max(0.0, min(65535.0, (a / 128.0) * 32768.0 + 32768.0)) + 0.5)
        b16 = int(max(0.0, min(65535.0, (bstar / 128.0) * 32768.0 + 32768.0)) + 0.5)
        planes[0] += struct.pack(">H", l16)
        planes[1] += struct.pack(">H", a16)
        planes[2] += struct.pack(">H", b16)
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


def planes_from_cmyk16(cmyk):
    planes = [bytearray(), bytearray(), bytearray(), bytearray()]
    for c, m, y, k in cmyk:
        planes[0] += struct.pack(">H", c * 257)
        planes[1] += struct.pack(">H", m * 257)
        planes[2] += struct.pack(">H", y * 257)
        planes[3] += struct.pack(">H", k * 257)
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


def build_psd_layer_only_single_rgb8(planes, *, color_mode=3, alpha_plane=None):
    if color_mode not in (3, 7, 9):
        raise RuntimeError("layer-only RGB8 fixture supports color_mode 3/7/9")
    if len(planes) < 3:
        raise RuntimeError("layer-only RGB fixture requires R/G/B planes")
    row_bytes = WIDTH
    plane_bytes = row_bytes * HEIGHT
    for i in range(3):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected RGB8 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha plane size")

    layer_planes = [
        (0, planes[0]),
        (1, planes[1]),
        (2, planes[2]),
    ]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, plane in layer_planes:
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 8)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_cmyk8(planes, *, color_mode=4, alpha_plane=None):
    if color_mode not in (4, 7):
        raise RuntimeError("layer-only CMYK8 fixture supports color_mode 4/7")
    if len(planes) < 4:
        raise RuntimeError("layer-only CMYK8 fixture requires C/M/Y/K planes")
    row_bytes = WIDTH
    plane_bytes = row_bytes * HEIGHT
    for i in range(4):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected CMYK8 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha plane size")

    layer_planes = [
        (0, planes[0]),
        (1, planes[1]),
        (2, planes[2]),
        (3, planes[3]),
    ]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, plane in layer_planes:
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 8)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_cmyk16(planes, *, color_mode=4, alpha_plane=None):
    if color_mode not in (4, 7):
        raise RuntimeError("layer-only CMYK16 fixture supports color_mode 4/7")
    if len(planes) < 4:
        raise RuntimeError("layer-only CMYK16 fixture requires C/M/Y/K planes")
    row_bytes = WIDTH * 2
    plane_bytes = row_bytes * HEIGHT
    for i in range(4):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected CMYK16 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha16 plane size")

    layer_planes = [
        (0, planes[0]),
        (1, planes[1]),
        (2, planes[2]),
        (3, planes[3]),
    ]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, plane in layer_planes:
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 16)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_cmyk32(planes, *, color_mode=4, alpha_plane=None):
    if color_mode not in (4, 7):
        raise RuntimeError("layer-only CMYK32 fixture supports color_mode 4/7")
    if len(planes) < 4:
        raise RuntimeError("layer-only CMYK32 fixture requires C/M/Y/K planes")
    row_bytes = WIDTH * 4
    plane_bytes = row_bytes * HEIGHT
    for i in range(4):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected CMYK32 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha32 plane size")

    layer_planes = [
        (0, planes[0]),
        (1, planes[1]),
        (2, planes[2]),
        (3, planes[3]),
    ]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, plane in layer_planes:
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 32)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_gray8(
    plane,
    *,
    color_mode=1,
    alpha_plane=None,
    color_mode_data=b"",
):
    if color_mode not in (1, 2, 8):
        raise RuntimeError("layer-only fixture supports color_mode 1/2/8")
    if color_mode == 2 and len(color_mode_data) < 768:
        raise RuntimeError("indexed layer-only fixture requires 768-byte palette")
    row_bytes = WIDTH
    plane_bytes = row_bytes * HEIGHT
    if len(plane) != plane_bytes:
        raise RuntimeError("unexpected Gray8 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha plane size")

    layer_planes = [(0, plane)]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, channel_plane in layer_planes:
        payload = struct.pack(">H", 0) + channel_plane
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 8)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", len(color_mode_data))
    out += color_mode_data
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_rgb16(planes, *, color_mode=3, alpha_plane=None):
    if color_mode not in (3, 7, 9):
        raise RuntimeError("layer-only RGB16 fixture supports color_mode 3/7/9")
    if len(planes) < 3:
        raise RuntimeError("layer-only RGB16 fixture requires R/G/B planes")
    row_bytes = WIDTH * 2
    plane_bytes = row_bytes * HEIGHT
    for i in range(3):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected RGB16 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha16 plane size")

    layer_planes = [
        (0, planes[0]),
        (1, planes[1]),
        (2, planes[2]),
    ]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, plane in layer_planes:
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 16)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_gray16(
    plane,
    *,
    color_mode=1,
    alpha_plane=None,
):
    if color_mode not in (1, 8):
        raise RuntimeError("layer-only Gray16 fixture supports color_mode 1/8")
    row_bytes = WIDTH * 2
    plane_bytes = row_bytes * HEIGHT
    if len(plane) != plane_bytes:
        raise RuntimeError("unexpected Gray16 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha16 plane size")

    layer_planes = [(0, plane)]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, channel_plane in layer_planes:
        payload = struct.pack(">H", 0) + channel_plane
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 16)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_rgb32(planes, *, color_mode=3, alpha_plane=None):
    if color_mode not in (3, 7, 9):
        raise RuntimeError("layer-only RGB32 fixture supports color_mode 3/7/9")
    if len(planes) < 3:
        raise RuntimeError("layer-only RGB32 fixture requires R/G/B planes")
    row_bytes = WIDTH * 4
    plane_bytes = row_bytes * HEIGHT
    for i in range(3):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected RGB32 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha32 plane size")

    layer_planes = [
        (0, planes[0]),
        (1, planes[1]),
        (2, planes[2]),
    ]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, plane in layer_planes:
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 32)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_single_gray32(
    plane,
    *,
    color_mode=1,
    alpha_plane=None,
):
    if color_mode not in (1, 8):
        raise RuntimeError("layer-only Gray32 fixture supports color_mode 1/8")
    row_bytes = WIDTH * 4
    plane_bytes = row_bytes * HEIGHT
    if len(plane) != plane_bytes:
        raise RuntimeError("unexpected Gray32 plane size")
    if alpha_plane is not None and len(alpha_plane) != plane_bytes:
        raise RuntimeError("unexpected alpha32 plane size")

    layer_planes = [(0, plane)]
    if alpha_plane is not None:
        layer_planes.append((-1, alpha_plane))

    layer_record = bytearray()
    channel_payload = bytearray()
    layer_record += struct.pack(">iiii", 0, 0, HEIGHT, WIDTH)
    layer_record += struct.pack(">H", len(layer_planes))
    for channel_id, channel_plane in layer_planes:
        payload = struct.pack(">H", 0) + channel_plane
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
    out += struct.pack(">H", len(layer_planes))
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", 32)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", 0)  # color mode data length
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted (missing merged/composite image).
    return bytes(out)


def build_psd_layer_only_multilayer_rgb8(planes):
    if len(planes) < 3:
        raise RuntimeError("multilayer RGB fixture requires R/G/B planes")
    row_bytes = WIDTH
    plane_bytes = row_bytes * HEIGHT
    for i in range(3):
        if len(planes[i]) != plane_bytes:
            raise RuntimeError("unexpected RGB8 plane size")

    # Minimal marker for "multiple layers exist". Builtin fallback rejects this
    # layout before layer record parsing (requires exactly one layer).
    layer_info = struct.pack(">h", 2)
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
    # Composite image data intentionally omitted.
    return bytes(out)


def _pack_layer_pascal_name(name: bytes = b""):
    if len(name) > 255:
        raise RuntimeError("layer name too long")
    data = bytes([len(name)]) + name
    pad = (-len(data)) & 3
    return data + (b"\x00" * pad)


def build_psd_layer_only_multilayer_custom(
    *,
    color_mode: int,
    depth: int,
    channels_header: int,
    color_mode_data: bytes,
    layers,
):
    if depth == 8:
        sample_bytes = 1
    elif depth == 16:
        sample_bytes = 2
    elif depth == 32:
        sample_bytes = 4
    else:
        raise RuntimeError(f"unsupported depth: {depth}")

    layer_records = bytearray()
    channel_payload = bytearray()
    for layer in layers:
        top = int(layer.get("top", 0))
        left = int(layer.get("left", 0))
        bottom = int(layer.get("bottom", HEIGHT))
        right = int(layer.get("right", WIDTH))
        if bottom < top or right < left:
            raise RuntimeError("invalid layer geometry")
        layer_w = right - left
        layer_h = bottom - top
        row_bytes = layer_w * sample_bytes
        expected_plane_bytes = row_bytes * layer_h

        channel_ids = list(layer["channel_ids"])
        planes = list(layer["planes"])
        if len(channel_ids) != len(planes):
            raise RuntimeError("channel_ids and planes length mismatch")

        layer_records += struct.pack(">iiii", top, left, bottom, right)
        layer_records += struct.pack(">H", len(channel_ids))
        for channel_id, plane in zip(channel_ids, planes):
            if len(plane) != expected_plane_bytes:
                raise RuntimeError("layer plane size mismatch")
            payload = struct.pack(">H", 0) + plane
            layer_records += struct.pack(">hI", int(channel_id), len(payload))
            channel_payload += payload

        blend_key = layer.get("blend_key", b"norm")
        if isinstance(blend_key, str):
            blend_key = blend_key.encode("ascii")
        if len(blend_key) != 4:
            raise RuntimeError("blend_key must be 4 bytes")
        opacity = int(layer.get("opacity", 255)) & 0xFF
        clipping = int(layer.get("clipping", 0)) & 0xFF
        flags = int(layer.get("flags", 0)) & 0xFF
        extra = bytearray()
        extra += struct.pack(">I", 0)  # layer mask data length
        extra += struct.pack(">I", 0)  # blending ranges length
        extra += _pack_layer_pascal_name(layer.get("name", b""))
        for key, data in layer.get("additional_blocks", []):
            if isinstance(key, str):
                key = key.encode("ascii")
            if len(key) != 4:
                raise RuntimeError("additional block key must be 4 bytes")
            extra += b"8BIM" + key + struct.pack(">I", len(data)) + data
            if len(data) & 1:
                extra += b"\x00"
        layer_records += b"8BIM" + blend_key
        layer_records += bytes([opacity, clipping, flags, 0])
        layer_records += struct.pack(">I", len(extra))
        layer_records += extra

    layer_info = bytearray()
    layer_info += struct.pack(">h", len(layers))
    layer_info += layer_records
    layer_info += channel_payload
    layer_and_mask = struct.pack(">I", len(layer_info)) + layer_info

    out = bytearray()
    out += b"8BPS"
    out += struct.pack(">H", 1)
    out += b"\x00" * 6
    out += struct.pack(">H", channels_header)
    out += struct.pack(">I", HEIGHT)
    out += struct.pack(">I", WIDTH)
    out += struct.pack(">H", depth)
    out += struct.pack(">H", color_mode)
    out += struct.pack(">I", len(color_mode_data))
    out += color_mode_data
    out += struct.pack(">I", 0)  # image resources length
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    return bytes(out)


def build_rgb8_patch_plane(value: int, top: int, left: int, bottom: int, right: int):
    w = right - left
    h = bottom - top
    return bytes([value] * (w * h))


def build_u16_patch_plane(value: int, top: int, left: int, bottom: int, right: int):
    w = right - left
    h = bottom - top
    sample = struct.pack(">H", value & 0xFFFF)
    return sample * (w * h)


def build_f32_patch_plane(value: float, top: int, left: int, bottom: int, right: int):
    w = right - left
    h = bottom - top
    sample = struct.pack(">f", float(value))
    return sample * (w * h)


def build_psd_missing_composite_marker(
    *,
    channels,
    depth,
    color_mode,
    color_mode_data=b"",
):
    layer_and_mask = struct.pack(">I", 4) + b"\x00\x00\x00\x00"

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
    out += struct.pack(">I", len(layer_and_mask))
    out += layer_and_mask
    out += struct.pack(">H", 0)  # compression for composite image data
    # Composite image data intentionally omitted.
    return bytes(out)


def write_file(path: pathlib.Path, data: bytes):
    path.write_bytes(data)
    print(path)


def mutate_first_additional_block_length(
    data: bytes,
    *,
    key: bytes,
    malformed_length: int,
) -> bytes:
    marker = b"8BIM" + key
    marker_offset = data.find(marker)
    if marker_offset < 0:
        raise RuntimeError(f"additional block marker not found: {key!r}")
    if len(key) != 4:
        raise RuntimeError("additional block key must be 4 bytes")
    out = bytearray(data)
    out[marker_offset + 8:marker_offset + 12] = struct.pack(">I", malformed_length)
    return bytes(out)


def mutate_first_additional_block_type(
    data: bytes,
    *,
    key: bytes,
    expected_type: bytes,
    mutated_type: bytes,
) -> bytes:
    marker = b"8BIM" + key
    marker_offset = data.find(marker)
    if marker_offset < 0:
        raise RuntimeError(f"additional block marker not found: {key!r}")
    if len(key) != 4:
        raise RuntimeError("additional block key must be 4 bytes")
    if len(expected_type) != 4 or len(mutated_type) != 4:
        raise RuntimeError("descriptor type must be 4 bytes")
    payload_length = struct.unpack_from(">I", data, marker_offset + 8)[0]
    payload_offset = marker_offset + 12
    payload_end = payload_offset + payload_length
    if payload_end > len(data):
        raise RuntimeError("invalid additional block payload length")
    type_offset = data.find(expected_type, payload_offset, payload_end)
    if type_offset < 0:
        raise RuntimeError(
            f"descriptor type not found in additional block: {expected_type!r}"
        )
    out = bytearray(data)
    out[type_offset:type_offset + 4] = mutated_type
    return bytes(out)


def write_descriptor_malformed_fixtures(out_dir: pathlib.Path):
    # Corrupt the fill additional-block length so parser hits deterministic
    # malformed layer extra-data diagnostics.
    malformed_length = 0x7FFFFFFF
    variants = [
        (
            "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor.psd",
            b"SoCo",
            "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_malformed.psd",
        ),
        (
            "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor.psd",
            b"GdFl",
            "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_malformed.psd",
        ),
        (
            "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor.psd",
            b"PtFl",
            "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_malformed.psd",
        ),
    ]
    for base_name, key, malformed_name in variants:
        base = (out_dir / base_name).read_bytes()
        write_file(
            out_dir / malformed_name,
            mutate_first_additional_block_length(
                base,
                key=key,
                malformed_length=malformed_length,
            ),
        )
    invalid_payload_variants = [
        (
            "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor.psd",
            b"SoCo",
            "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_invalid_payload.psd",
        ),
        (
            "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor.psd",
            b"GdFl",
            "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_invalid_payload.psd",
        ),
        (
            "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor.psd",
            b"PtFl",
            "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_invalid_payload.psd",
        ),
    ]
    for base_name, key, invalid_name in invalid_payload_variants:
        base = (out_dir / base_name).read_bytes()
        write_file(
            out_dir / invalid_name,
            mutate_first_additional_block_type(
                base,
                key=key,
                expected_type=b"Objc",
                mutated_type=b"BADC",
            ),
        )


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
    lab16_planes = planes_from_lab16(lab)
    # Keep Lab32 chroma small so builtin ZIP+prediction decode and
    # coregraphics raw decode stay comparable in cross-loader LSQA.
    lab32_chroma_scale = 0.02
    lab32_values = [(l, a * lab32_chroma_scale, b * lab32_chroma_scale)
                    for (l, a, b) in lab]
    lab32_planes = planes_from_lab32(lab32_values)

    cmyk8_planes = planes_from_cmyk8(cmyk8)
    cmyk16_planes = planes_from_cmyk16(cmyk8)
    cmyk32_planes = planes_from_cmyk32(cmyk8)

    bitmap_plane = [build_bitmap_plane(bitmap)]

    alpha8_plane = build_alpha8_fade_plane()
    alpha16_plane = expand_u8_plane_to_u16be(alpha8_plane)
    alpha32_plane = expand_u8_plane_to_f32be(alpha8_plane)
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
    write_file(
        out_dir / "snake16_rgb8_alpha_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb8(rgb8_planes, alpha_plane=alpha8_plane),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer.psd",
        build_psd_layer_only_multilayer_rgb8(rgb8_planes),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque blue patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas blue clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        bytes([0] * (WIDTH * HEIGHT)),
                        bytes([0] * (WIDTH * HEIGHT)),
                        bytes([255] * (WIDTH * HEIGHT)),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: red patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, -2],
                    "planes": [
                        bytes([0] * (WIDTH * HEIGHT)),
                        bytes([255] * (WIDTH * HEIGHT)),
                        bytes([0] * (WIDTH * HEIGHT)),
                        alpha8_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_nonpixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_multilayer_nonpixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_soco.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"SoCo", build_sxfl_soco_payload(255, 48, 64))],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"SoCo", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_cmyk(
                                0.0,
                                81.17647058823529,
                                74.90196078431373,
                                0.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"SoCo", build_descriptor_soco_payload_gray(50.0))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_hsb(
                                355.3623188405797,
                                81.17647058823529,
                                100.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_soco_descriptor_lab.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_lab(
                                53.389,
                                0.0,
                                0.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_gdfl.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_sxfl_gdfl_payload(
                                gradient_type=0,
                                reverse=False,
                                angle_deg=0.0,
                                scale=1.0,
                                stops=[
                                    (0.0, 255, 32, 32, 255),
                                    (1.0, 32, 64, 255, 255),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, 255, 32, 32, 100.0),
                                    (1.0, 32, 64, 255, 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (
                                        0.0,
                                        ("cmyk", 0.0, 81.17647058823529, 74.90196078431373, 0.0),
                                        100.0,
                                    ),
                                    (
                                        1.0,
                                        ("cmyk", 0.0, 81.17647058823529, 74.90196078431373, 0.0),
                                        100.0,
                                    ),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, ("gray", 50.0), 100.0),
                                    (1.0, ("gray", 50.0), 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (
                                        0.0,
                                        ("hsb", 355.3623188405797, 81.17647058823529, 100.0),
                                        100.0,
                                    ),
                                    (
                                        1.0,
                                        ("hsb", 355.3623188405797, 81.17647058823529, 100.0),
                                        100.0,
                                    ),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_lab.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, ("lab", 53.389, 0.0, 0.0), 100.0),
                                    (1.0, ("lab", 53.389, 0.0, 0.0), 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_rgb8_missing_composite_multilayer_fill_gdfl_descriptor_grad_nested.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, 255, 32, 32, 100.0),
                                    (1.0, 32, 64, 255, 100.0),
                                ],
                                use_nested_grad=True,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_ptfl.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_sxfl_ptfl_payload(
                                tile=4,
                                fg_rgb=(250, 250, 250),
                                bg_rgb=(30, 30, 30),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=(250, 250, 250),
                                bg_rgb=(30, 30, 30),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("gray", 98.0392156862745),
                                bg_rgb=("gray", 11.764705882352942),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("hsb", 0.0, 0.0, 98.0392156862745),
                                bg_rgb=("hsb", 0.0, 0.0, 11.764705882352942),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_fill_ptfl_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("cmyk", 0.0, 0.0, 0.0, 1.9607843137254901),
                                bg_rgb=("cmyk", 0.0, 0.0, 0.0, 88.23529411764706),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb8_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
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
    write_file(
        out_dir / "snake16_gray8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray8(gray8_plane),
    )
    write_file(
        out_dir / "snake16_gray8_alpha_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray8(gray8_plane, alpha_plane=alpha8_plane),
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
    write_file(
        out_dir / "snake16_duotone8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray8(gray8_plane, color_mode=8),
    )
    write_file(
        out_dir / "snake16_duotone8_alpha_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray8(
            gray8_plane,
            color_mode=8,
            alpha_plane=alpha8_plane,
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
    write_file(
        out_dir / "snake16_indexed8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray8(
            indexed_planes[0],
            color_mode=2,
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
    write_file(
        out_dir / "snake16_lab8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb8(lab8_planes, color_mode=9),
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
    write_file(
        out_dir / "snake16_rgb16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb16(rgb16_planes),
    )
    write_file(
        out_dir / "snake16_rgb16_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb16_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb16_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, -2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        alpha16_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb16_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=3,
            depth=16,
            color_mode=3,
        ),
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
        out_dir / "snake16_gray16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray16(gray16_plane),
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
        out_dir / "snake16_duotone16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray16(gray16_plane, color_mode=8),
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

    write_variants(
        out_dir,
        "snake16_lab16",
        lab16_planes,
        channels=3,
        depth=16,
        color_mode=9,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_lab16_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=3,
            depth=16,
            color_mode=9,
        ),
    )
    write_file(
        out_dir / "snake16_lab16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb16(lab16_planes, color_mode=9),
    )
    write_file(
        out_dir / "snake16_lab16_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=9,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque neutral black in Lab16
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                        build_u16_patch_plane(0x8000, 4, 4, 12, 12),
                        build_u16_patch_plane(0x8000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": lab16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )

    write_variants(
        out_dir,
        "snake16_cmyk16",
        cmyk16_planes,
        channels=4,
        depth=16,
        color_mode=4,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=4,
            depth=16,
            color_mode=4,
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_cmyk16(cmyk16_planes),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch (PSD CMYK polarity is inverted)
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3, -2],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        alpha16_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk16_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
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
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_rgb32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb32(rgb32_planes),
    )
    write_file(
        out_dir / "snake16_rgb32_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=32,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb32_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=32,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_rgb32_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=3,
            depth=32,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, -2],
                    "planes": [
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        alpha32_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
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
    write_file(
        out_dir / "snake16_gray32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray32(gray32_plane),
    )
    write_file(
        out_dir / "snake16_gray32_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=1,
            depth=32,
            color_mode=1,
        ),
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
    write_file(
        out_dir / "snake16_lab32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb32(lab32_planes, color_mode=9),
    )
    write_file(
        out_dir / "snake16_duotone32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_gray32(gray32_plane, color_mode=8),
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
        out_dir / "snake16_cmyk8_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=4,
            depth=8,
            color_mode=4,
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_cmyk8(cmyk8_planes),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch (PSD CMYK polarity inverted)
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(0, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3, -2],
                    "planes": [
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(0, 0, 0, HEIGHT, WIDTH),
                        alpha8_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_soco.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"SoCo", build_sxfl_soco_payload(255, 48, 64))],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_soco_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"SoCo", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_soco_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_cmyk(
                                0.0,
                                81.17647058823529,
                                74.90196078431373,
                                0.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_soco_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"SoCo", build_descriptor_soco_payload_gray(50.0))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_soco_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_hsb(
                                355.3623188405797,
                                81.17647058823529,
                                100.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_soco_descriptor_lab.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_lab(
                                53.389,
                                0.0,
                                0.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, 255, 32, 32, 100.0),
                                    (1.0, 32, 64, 255, 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (
                                        0.0,
                                        ("cmyk", 0.0, 81.17647058823529, 74.90196078431373, 0.0),
                                        100.0,
                                    ),
                                    (
                                        1.0,
                                        ("cmyk", 0.0, 81.17647058823529, 74.90196078431373, 0.0),
                                        100.0,
                                    ),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, ("gray", 50.0), 100.0),
                                    (1.0, ("gray", 50.0), 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (
                                        0.0,
                                        ("hsb", 355.3623188405797, 81.17647058823529, 100.0),
                                        100.0,
                                    ),
                                    (
                                        1.0,
                                        ("hsb", 355.3623188405797, 81.17647058823529, 100.0),
                                        100.0,
                                    ),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_lab.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, ("lab", 53.389, 0.0, 0.0), 100.0),
                                    (1.0, ("lab", 53.389, 0.0, 0.0), 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_grad_nested.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, 255, 32, 32, 100.0),
                                    (1.0, 32, 64, 255, 100.0),
                                ],
                                use_nested_grad=True,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=(250, 250, 250),
                                bg_rgb=(30, 30, 30),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("gray", 98.0392156862745),
                                bg_rgb=("gray", 11.764705882352942),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("hsb", 0.0, 0.0, 98.0392156862745),
                                bg_rgb=("hsb", 0.0, 0.0, 11.764705882352942),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("cmyk", 0.0, 0.0, 0.0, 1.9607843137254901),
                                bg_rgb=("cmyk", 0.0, 0.0, 0.0, 88.23529411764706),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk8_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
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

    write_variants(
        out_dir,
        "snake16_cmyk32",
        cmyk32_planes,
        channels=4,
        depth=32,
        color_mode=4,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=4,
            depth=32,
            color_mode=4,
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_cmyk32(cmyk32_planes),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch (PSD CMYK polarity inverted)
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3, -2],
                    "planes": [
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        alpha32_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_cmyk32_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=4,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )

    # Multichannel (mode=7) fixtures:
    # - channels=3 behaves like RGB
    # - channels=4 behaves like CMYK
    write_variants(
        out_dir,
        "snake16_mode7_rgb8",
        rgb8_planes,
        channels=3,
        depth=8,
        color_mode=7,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=3,
            depth=8,
            color_mode=7,
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb8(rgb8_planes, color_mode=7),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_rgb8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_fill_soco.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"SoCo", build_sxfl_soco_payload(255, 48, 64))],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb8_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_variants(
        out_dir,
        "snake16_mode7_rgb16",
        rgb16_planes,
        channels=3,
        depth=16,
        color_mode=7,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_mode7_rgb16_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=3,
            depth=16,
            color_mode=7,
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb16(rgb16_planes, color_mode=7),
    )
    write_file(
        out_dir / "snake16_mode7_rgb16_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb16_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb16_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, -2],
                    "planes": [
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        alpha16_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_variants(
        out_dir,
        "snake16_mode7_rgb32",
        rgb32_planes,
        channels=3,
        depth=32,
        color_mode=7,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_mode7_rgb32_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=3,
            depth=32,
            color_mode=7,
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_rgb32(rgb32_planes, color_mode=7),
    )
    write_file(
        out_dir / "snake16_mode7_rgb32_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb32_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_rgb32_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=3,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, -2],
                    "planes": [
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        alpha32_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2],
                    "planes": rgb32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_variants(
        out_dir,
        "snake16_mode7_cmyk8",
        cmyk8_planes,
        channels=4,
        depth=8,
        color_mode=7,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=4,
            depth=8,
            color_mode=7,
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_single_layer.psd",
        build_psd_layer_only_single_cmyk8(cmyk8_planes, color_mode=7),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch (PSD CMYK polarity inverted)
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(0, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3, -2],
                    "planes": [
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(255, 0, 0, HEIGHT, WIDTH),
                        build_rgb8_patch_plane(0, 0, 0, HEIGHT, WIDTH),
                        alpha8_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_soco.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"SoCo", build_sxfl_soco_payload(255, 48, 64))],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_soco_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"SoCo", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_soco_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_cmyk(
                                0.0,
                                81.17647058823529,
                                74.90196078431373,
                                0.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_soco_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"SoCo", build_descriptor_soco_payload_gray(50.0))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_soco_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_hsb(
                                355.3623188405797,
                                81.17647058823529,
                                100.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_soco_descriptor_lab.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"SoCo",
                            build_descriptor_soco_payload_lab(
                                53.389,
                                0.0,
                                0.0,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, 255, 32, 32, 100.0),
                                    (1.0, 32, 64, 255, 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (
                                        0.0,
                                        ("cmyk", 0.0, 81.17647058823529, 74.90196078431373, 0.0),
                                        100.0,
                                    ),
                                    (
                                        1.0,
                                        ("cmyk", 0.0, 81.17647058823529, 74.90196078431373, 0.0),
                                        100.0,
                                    ),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, ("gray", 50.0), 100.0),
                                    (1.0, ("gray", 50.0), 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (
                                        0.0,
                                        ("hsb", 355.3623188405797, 81.17647058823529, 100.0),
                                        100.0,
                                    ),
                                    (
                                        1.0,
                                        ("hsb", 355.3623188405797, 81.17647058823529, 100.0),
                                        100.0,
                                    ),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_lab.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, ("lab", 53.389, 0.0, 0.0), 100.0),
                                    (1.0, ("lab", 53.389, 0.0, 0.0), 100.0),
                                ],
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_gdfl_descriptor_grad_nested.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"GdFl",
                            build_descriptor_gdfl_payload(
                                gradient_type_key=b"Lnr ",
                                reverse=False,
                                angle_deg=0.0,
                                scale_percent=100.0,
                                stops=[
                                    (0.0, 255, 32, 32, 100.0),
                                    (1.0, 32, 64, 255, 100.0),
                                ],
                                use_nested_grad=True,
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=(250, 250, 250),
                                bg_rgb=(30, 30, 30),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_gray.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("gray", 98.0392156862745),
                                bg_rgb=("gray", 11.764705882352942),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_hsb.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("hsb", 0.0, 0.0, 98.0392156862745),
                                bg_rgb=("hsb", 0.0, 0.0, 11.764705882352942),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_cmyk.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (
                            b"PtFl",
                            build_descriptor_ptfl_payload(
                                tile=4,
                                fg_rgb=("cmyk", 0.0, 0.0, 0.0, 1.9607843137254901),
                                bg_rgb=("cmyk", 0.0, 0.0, 0.0, 88.23529411764706),
                            ),
                        )
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(255, 4, 4, 12, 12),
                        build_rgb8_patch_plane(0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk8_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=8,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk8_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_variants(
        out_dir,
        "snake16_mode7_cmyk16",
        cmyk16_planes,
        channels=4,
        depth=16,
        color_mode=7,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=4,
            depth=16,
            color_mode=7,
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_single_layer.psd",
        build_psd_layer_only_single_cmyk16(cmyk16_planes, color_mode=7),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch (PSD CMYK polarity inverted)
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk16_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0x0000, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                        build_u16_patch_plane(0xFFFF, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk16_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=16,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3, -2],
                    "planes": [
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0xFFFF, 0, 0, HEIGHT, WIDTH),
                        build_u16_patch_plane(0x0000, 0, 0, HEIGHT, WIDTH),
                        alpha16_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk16_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_variants(
        out_dir,
        "snake16_mode7_cmyk32",
        cmyk32_planes,
        channels=4,
        depth=32,
        color_mode=7,
        variants=["raw", "rle", "zip", "zip_pred"],
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_marker.psd",
        build_psd_missing_composite_marker(
            channels=4,
            depth=32,
            color_mode=7,
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_single_layer.psd",
        build_psd_layer_only_single_cmyk32(cmyk32_planes, color_mode=7),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_normal.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer: opaque black patch (PSD CMYK polarity inverted)
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    # bottom layer: snake base
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [(b"TySh", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir
        / "snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_descriptor.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [],
                    "planes": [],
                    "blend_key": b"norm",
                    "additional_blocks": [
                        (b"TySh", build_descriptor_soco_payload(255, 48, 64))
                    ],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_unknown_blend.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(0.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"zzzz",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_vector_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"vmsk", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_layer_effects.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"lfx2", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_knockout.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                    "additional_blocks": [(b"knko", b"\x00")],
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_clipping.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top clipped layer: full-canvas black clipped by base below
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                    ],
                    "blend_key": b"norm",
                    "clipping": 1,
                },
                {
                    # clipping base: opaque white patch
                    "top": 4,
                    "left": 4,
                    "bottom": 12,
                    "right": 12,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": [
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                        build_f32_patch_plane(1.0, 4, 4, 12, 12),
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
    )
    write_file(
        out_dir / "snake16_mode7_cmyk32_missing_composite_multilayer_mask.psd",
        build_psd_layer_only_multilayer_custom(
            color_mode=7,
            depth=32,
            channels_header=4,
            color_mode_data=b"",
            layers=[
                {
                    # top layer with raster user mask channel (-2)
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3, -2],
                    "planes": [
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(1.0, 0, 0, HEIGHT, WIDTH),
                        build_f32_patch_plane(0.0, 0, 0, HEIGHT, WIDTH),
                        alpha32_plane,
                    ],
                    "blend_key": b"norm",
                },
                {
                    "top": 0,
                    "left": 0,
                    "bottom": HEIGHT,
                    "right": WIDTH,
                    "channel_ids": [0, 1, 2, 3],
                    "planes": cmyk32_planes,
                    "blend_key": b"norm",
                },
            ],
        ),
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
    write_descriptor_malformed_fixtures(out_dir)


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
