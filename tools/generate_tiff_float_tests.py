#!/usr/bin/env python3
"""Literal IEEEFP32 TIFF samples and independent rational polarity values."""
from fractions import Fraction as F
import hashlib
import json
import struct

# Sign boundaries, subnormal/normal edges, neighbors of one, rounding ties,
# large magnitudes and ordinary fractions. These are prescribed storage words.
WORDS = [
    0x00000000, 0x80000000, 0xbe800000, 0x3f000000, 0x3fa00000,
    0x00000001, 0x80000001, 0x007fffff, 0x807fffff, 0x00800000,
    0x80800000, 0x3f7fffff, 0x3f800000, 0x3f800001, 0x33000000,
    0xb3000000, 0x33800000, 0xb3800000, 0x4b800000, 0xcb800000,
    0x4c000000, 0x7f7fffff, 0xff7fffff, 0x3e800000,
]


def rational(word):
    exponent = (word >> 23) & 255
    mantissa = word & 0x7fffff
    assert exponent != 255
    significand = mantissa if exponent == 0 else mantissa + (1 << 23)
    value = F(significand) * F(2) ** (-149 if exponent == 0 else exponent-150)
    return -value if word >> 31 else value


def even(numerator, denominator):
    quotient, remainder = divmod(numerator, denominator)
    return quotient + (2*remainder > denominator or
                       2*remainder == denominator and quotient % 2 != 0)


def nearest(value):
    """Round a rational directly to binary32; no intermediate float."""
    if value == 0:
        return 0
    sign = 0x80000000 if value < 0 else 0
    value = abs(value)
    numerator, denominator = value.numerator, value.denominator
    exponent = numerator.bit_length() - denominator.bit_length()
    if value < F(2)**exponent:
        exponent -= 1
    if exponent < -126:
        return sign | even(numerator << 149, denominator)
    scaled = value / F(2)**(exponent-23)
    rounded = even(scaled.numerator, scaled.denominator)
    if rounded == 1 << 24:
        rounded >>= 1
        exponent += 1
    assert exponent <= 127
    return sign | ((exponent+127) << 23) | (rounded - (1 << 23))


def neighbor(value):
    """Independent nearest neighbor using monotone positive float words."""
    if value == 0:
        return 0
    sign = 0x80000000 if value < 0 else 0
    value = abs(value)
    low, high = 0, 0x7f7fffff
    while low < high:
        middle = (low+high+1) // 2
        if rational(middle) <= value:
            low = middle
        else:
            high = middle-1
    if low == 0x7f7fffff:
        # The only over-range value here is FLT_MAX+1, which rounds down.
        assert value - rational(low) < F(2)**103
        return sign | low
    below, above = rational(low), rational(low+1)
    choose_low = (value-below < above-value or
                  value-below == above-value and low % 2 == 0)
    return sign | (low if choose_low else low+1)


assert all(nearest(rational(w)) == w or w == 0x80000000 for w in WORDS)
WHITE = [nearest(1-rational(w)) for w in WORDS]
assert WHITE == [neighbor(1-rational(w)) for w in WORDS]


def write_tiff(raw, photo, endian='<', orientation=1, rows=1,
               icc=None, metadata=False, bits=32, height=None):
    """Serialize uncompressed words directly, without a TIFF or CMS library."""
    samples = 3 if photo == 2 else 1
    width = 4 if samples == 3 else 6
    height = height if height is not None else 2 if samples == 3 else 4
    assert bits in (16, 32)
    assert len(raw) == width*height*samples
    chunks = [b''.join(struct.pack(endian+('H' if bits == 16 else 'I'), value) for value in
                      raw[y*width*samples:min(height, y+rows)*width*samples])
              for y in range(0, height, rows)]
    tags = {256: (4, [width]), 257: (4, [height]), 258: (3, [bits]*samples),
            259: (3, [1]), 262: (3, [photo]), 273: (4, [0]*len(chunks)),
            274: (3, [orientation]), 277: (3, [samples]), 278: (4, [rows]),
            279: (4, [len(chunk) for chunk in chunks]), 284: (3, [1]),
            339: (3, [1 if bits == 16 else 3]*samples)}
    if icc is not None:
        tags[34675] = (7, icc)
    if metadata:
        tags[340] = (11, [0xff7fffff]*samples)
        tags[341] = (11, [0x7f7fffff]*samples)
        tags[318] = (5, [3127, 10000, 3290, 10000])
        tags[319] = (5, [64, 100, 33, 100, 30, 100, 60, 100, 15, 100, 6, 100])

    def header(offsets):
        tags[273] = (4, offsets)
        start = 8 + 2 + len(tags)*12 + 4
        table, payload = [], bytearray()
        for tag, (kind, values) in sorted(tags.items()):
            data = values if kind == 7 else struct.pack(
                endian + ('H' if kind == 3 else 'I')*len(values), *values)
            count = len(values)//2 if kind == 5 else len(values)
            field = (data.ljust(4, b'\0') if len(data) <= 4 else
                     struct.pack(endian+'I', start+len(payload)))
            table.append(struct.pack(endian+'HHI', tag, kind, count)+field)
            if len(data) > 4:
                payload.extend(data)
                payload.extend(bytes(len(payload) % 2))
        return ((b'II' if endian == '<' else b'MM') +
                struct.pack(endian+'HIH', 42, 8, len(tags)) +
                b''.join(table) + bytes(4) + payload)

    position = len(header([0]*len(chunks)))
    offsets = []
    for chunk in chunks:
        offsets.append(position)
        position += len(chunk)
    return header(offsets) + b''.join(chunks)


def expected(raw, photo, orientation=1):
    samples = 3 if photo == 2 else 1
    width, height = (4, 2) if samples == 3 else (6, 4)
    output = [0]*(width*height*3)
    for y in range(height):
        for x in range(width):
            dx = width-1-x if orientation in (2, 3) else x
            dy = height-1-y if orientation in (3, 4) else y
            if photo == 2:
                values = raw[(y*width+x)*3:(y*width+x+1)*3]
            else:
                value = raw[y*width+x]
                value = nearest(1-rational(value)) if photo == 0 else value
                values = [value]*3
            output[(dy*width+dx)*3:(dy*width+dx+1)*3] = values
    return output


def build(profile, cmyk):
    outputs = {}
    records = []

    def add(name, photo, endian='<', orientation=1, rows=1, poison=None,
            icc=None, metadata=False):
        raw = WORDS.copy()
        if poison is not None:
            raw[-1] = poison
        data = write_tiff(raw, photo, endian, orientation, rows, icc, metadata)
        outputs['tiff-float-' + name + '.tif'] = data
        frame_words = expected(raw, photo, orientation) if poison is None else None
        records.append(dict(name=name, photo=photo, endian=endian,
            orientation=orientation, rows=rows, raw=[f'{v:08x}' for v in raw],
            expected=[f'{v:08x}' for v in frame_words] if frame_words is not None else None,
            sha256=hashlib.sha256(data).hexdigest()))

    for name, photo in [('rgb', 2), ('black', 1), ('white', 0)]:
        for suffix, endian in [('le', '<'), ('be', '>')]:
            add(name+'-'+suffix, photo, endian, rows=2 if photo==0 else 1)
    # One representative per nonfinite kind and photometric branch.
    add('rgb-nan', 2, poison=0x7fc12345)
    add('black-pinf', 1, poison=0x7f800000)
    add('white-ninf', 0, '>', rows=2, poison=0xff800000)
    add('rgb-o3', 2, '>', orientation=3)
    add('black-o2', 1, '>', orientation=2)
    add('white-o4', 0, orientation=4, rows=2)
    add('rgb-range', 2, metadata=True)
    add('rgb-none-icc', 2, icc=profile)
    add('rgb-invalid', 2, icc=b'bad')
    add('black-invalid', 1, icc=b'bad')
    add('white-mismatch', 0, icc=cmyk)

    def array(name, values):
        return ('static uint32_t const '+name+'[24] = {\n' +
                ',\n'.join('    0x%08xu' % v for v in values) + '\n};\n')

    header = ('/* Generated offline: literal words and exact rational 1-x. */\n'
              '#ifndef SIXEL_TIFF_FLOAT_VALUES_H\n'
              '#define SIXEL_TIFF_FLOAT_VALUES_H\n#include <stdint.h>\n' +
              array('tuf_words', WORDS) + array('tuf_white', WHITE) + '#endif\n')
    outputs['tiff-float.h'] = header.encode()
    record = dict(generator='tools/generate_tiff_float_tests.py',
        rounding='Exact Fraction quotient/remainder ties-to-even, separately checked by binary-search nearest-neighbor distances',
        independent=True, raw=[f'{v:08x}' for v in WORDS],
        white=[f'{v:08x}' for v in WHITE],
        exact_white=[str(1-rational(v)) for v in WORDS], fixtures=records)
    outputs['tiff-float.json'] = (json.dumps(record, indent=2)+'\n').encode()
    return outputs
