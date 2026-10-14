#!/usr/bin/env python3
"""Derive legacy D65 Lab sample references from the authored raw planes.

These fixtures exercise libsixel's normalized CIE 1976 D65 Lab format.
Lab32 opponent planes are normalized coordinates, while L is in 0..100.
The old CoreGraphics rendering treated those opponent values as physical
Lab units and therefore cannot be an oracle for these native sample tests.
No production conversion routine or decoded libsixel image is used here.
"""
import argparse
import math
from pathlib import Path
import struct

DIRECTORY = Path(__file__).resolve().parent
INPUTS = DIRECTORY.parents[1] / 'inputs/formats'
# Standard rounded sRGB/D65 XYZ inverse, with the CIE 1931 D65 white.
XYZ_TO_RGB = ((3.2404542, -1.5371385, -.4985314),
              (-.9692660, 1.8760108, .0415560),
              (.0556434, -.2040259, 1.0572252))


def reference(depth, patch=False):
    data = (INPUTS / f'snake16_lab{depth}_raw.psd').read_bytes()
    assert struct.unpack_from('>HIIHH', data, 12) == (3, 16, 16, depth, 9)
    offset = 26
    for _ in range(3):
        offset += 4 + struct.unpack_from('>I', data, offset)[0]
    assert data[offset:offset + 2] == b'\0\0'
    planes = struct.unpack('>' + {8: 'B', 16: 'H', 32: 'f'}[depth] * 768,
                           data[offset + 2:])
    result = bytearray()
    for pixel in range(256):
        l, a, b = (planes[c * 256 + pixel] for c in range(3))
        if depth == 8:
            l, a, b = l * 100 / 255, a - 128, b - 128
        elif depth == 16:
            l, a, b = l * 100 / 65535, a / 256 - 128, b / 256 - 128
        else:
            assert l > 1.5 and abs(a) <= 1.5 and abs(b) <= 1.5
            a, b = a * 128, b * 128
        fy = (l + 16) / 116
        f = (fy + a / 500, fy, fy - b / 200)
        xyz = [white * (v ** 3 if v > 6 / 29
                        else 3 * (6 / 29) ** 2 * (v - 4 / 29))
               for v, white in zip(f, (.95047, 1, 1.08883))]
        for row in XYZ_TO_RGB:
            linear = sum(m * v for m, v in zip(row, xyz))
            linear = min(1, max(0, linear))
            gamma = 12.92 * linear if linear <= .0031308 else 1.055 * linear ** (1 / 2.4) - .055
            value = 0 if patch and 4 <= pixel % 16 < 12 and 4 <= pixel // 16 < 12 else math.floor(gamma * 65535 + .5)
            result.extend(struct.pack('>H', value))
    return b'P6\n16 16\n65535\n' + result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for depth, patch in ((8, False), (16, False), (32, False), (16, True)):
        suffix = '_multilayer' if patch else ''
        path = DIRECTORY / f'psd_snake16_lab{depth}{suffix}_d65_samples.ppm'
        data = reference(depth, patch)
        if args.check:
            assert path.read_bytes() == data, path
        else:
            path.write_bytes(data)


if __name__ == '__main__':
    main()
