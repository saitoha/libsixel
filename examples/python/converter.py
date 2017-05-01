#!/usr/bin/env python

import sys
from libsixel import *
from PIL import Image
from io import BytesIO

s = BytesIO()

file = sys.argv[1]
image = Image.open(file)
width, height = image.size
try:
    data = image.tobytes()
except NotImplementedError:
    data = image.tostring()
output = sixel_output_new(lambda data, s: s.write(data), s)

try:
    if image.mode == 'RGBA':
        dither = sixel_dither_new(256)
        sixel_dither_initialize(dither, data, width, height, SIXEL_PIXELFORMAT_RGBA8888)
    elif image.mode == 'RGB':
        dither = sixel_dither_new(256)
        sixel_dither_initialize(dither, data, width, height, SIXEL_PIXELFORMAT_RGB888)
    elif image.mode == 'P':
        palette = image.getpalette()
        dither = sixel_dither_new(256)
        sixel_dither_set_palette(dither, palette)
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8)
    elif image.mode == 'L':
        dither = sixel_dither_get(SIXEL_BUILTIN_G8)
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_G8)
    elif image.mode == '1':
        dither = sixel_dither_get(SIXEL_BUILTIN_G1)
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_G1)
    else:
        raise RuntimeError('unexpected image mode')
    try:
        sixel_encode(data, width, height, 1, dither, output)
        print(s.getvalue().decode('ascii'))
    finally:
        sixel_dither_unref(dither)
finally:
    sixel_output_unref(output)
