#!/usr/bin/env python3
"""Generate independent small PSD value oracles; never invoke libsixel.

Run offline with --check to verify the committed references. Weighted text
colors use the fixture's declared sRGB colors and run lengths in linear light.
Unknown blend mode falls back to Normal: paint the opaque magenta rectangle
over the unchanged raw RGB source planes, without a second image resampling.
"""

import argparse
import math
from pathlib import Path
import struct


DIRECTORY = Path(__file__).resolve().parent
SOURCE = DIRECTORY.parents[1] / 'inputs/formats/snake16_rgb8_raw.psd'


def linear(value):
    value /= 255
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def gamma(value):
    return 255 * (12.92 * value if value <= .0031308
                  else 1.055 * value ** (1 / 2.4) - .055)


def references():
    source = SOURCE.read_bytes()
    assert source[:6] == b'8BPS\0\1'
    assert struct.unpack_from('>HIIHH', source, 12) == (3, 16, 16, 8, 3)
    offset = 26
    for _ in range(3):
        length = struct.unpack_from('>I', source, offset)[0]
        offset += 4 + length
    assert source[offset:offset + 2] == b'\0\0'  # Raw planar image data.
    planes = source[offset + 2:]
    assert len(planes) == 3 * 256
    pixels = bytearray(planes[channel * 256 + i]
                       for i in range(256) for channel in range(3))
    for y in range(4, 12):
        for x in range(4, 12):
            pixels[(y * 16 + x) * 3:(y * 16 + x) * 3 + 3] = b'\xff\0\xff'
    yield 'psd_snake16_multilayer_unknown_blend_numeric.ppm', pixels

    colors = [(255, 48, 64), (32, 64, 255), (64, 192, 64)]
    for weights in ([1, 1], [1, 2, 1]):
        expected = [gamma(sum(linear(color[channel]) * weight
                              for color, weight in zip(colors, weights))
                          / sum(weights)) for channel in range(3)]
        pixels = bytes(math.floor(value + .5) for value in expected) * 256
        yield (f'psd_snake16_multilayer_fill_soco_weighted_'
               f'{len(weights)}run_numeric.ppm'), pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for name, pixels in references():
        data = b'P6\n16 16\n255\n' + pixels
        path = DIRECTORY / name
        if args.check:
            assert path.read_bytes() == data, path
        else:
            path.write_bytes(data)


if __name__ == '__main__':
    main()
