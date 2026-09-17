#!/usr/bin/env python3
"""Literal separate UINT16 sources and exact integer-only F32 oracles."""
import hashlib
import json

from generate_tiff_legacy_fixtures import directory, packed_row


def float_word(n, d=65535):
    """Round the rational directly to IEEE binary32, with ties to even."""
    if n == 0:
        return 0
    exponent = n.bit_length() - d.bit_length()
    if (n << max(0, -exponent)) < (d << max(0, exponent)):
        exponent -= 1
    quotient, remainder = divmod(n << (23 - exponent), d)
    if 2 * remainder > d or (2 * remainder == d and quotient % 2):
        quotient += 1
    if quotient == 1 << 24:
        quotient >>= 1
        exponent += 1
    return ((exponent + 127) << 23) | (quotient - (1 << 23))


def source_words(item):
    """Place literal integer samples in displayed coordinates before rounding."""
    output = [None] * 6
    for index, pixel in enumerate(item['samples']):
        x, y = index % 3, index // 3
        if item['orientation'] in (2, 3):
            x = 2 - x
        if item['orientation'] in (3, 4):
            y = 1 - y
        rgb = (pixel if item['photo'] == 2 else
               [65535 - pixel[0] if item['photo'] == 0 else pixel[0]] * 3)
        output[y * 3 + x] = [float_word(v) for v in rgb]
    return sum(output, [])


def literal(item, profile):
    """Emit two-row strips, with one PackBits literal packet per source row."""
    channels = 3 if item['photo'] == 2 else 1
    tags = {256: (4, [3]), 257: (4, [2]), 258: (3, [16] * channels),
            259: (3, [item['compression']]), 262: (3, [item['photo']]),
            274: (3, [item['orientation']]), 277: (3, [channels]),
            284: (3, [item['planar']]),
            339: (3, [item['sample_format']] * channels)}
    if profile is not None:
        tags[34675] = (7, profile)
    parts = []
    for plane in range(channels if item['planar'] == 2 else 1):
        rows = []
        for y in range(2):
            pixels = item['samples'][y * 3:(y + 1) * 3]
            values = ([pixel[plane] for pixel in pixels]
                      if item['planar'] == 2 else sum(pixels, []))
            row = packed_row(values, 16)
            rows.append(bytes([len(row) - 1]) + row
                        if item['compression'] == 32773 else row)
        parts.append(b''.join(rows))
    tags.update({273: (4, [0] * len(parts)), 278: (4, [2]),
                 279: (4, [len(part) for part in parts])})
    offset = len(directory(tags))
    offsets = []
    for part in parts:
        offsets.append(offset)
        offset += len(part)
    tags[273] = (4, offsets)
    return directory(tags) + b''.join(parts)


def build(p3, gray):
    rgb = [[0, 1, 256], [257, 32897, 16449], [65535, 32768, 8225],
           [129, 65534, 128], [32767, 128, 65535], [32896, 16448, 8224]]
    # The ordering is part of the small C observer's source table.
    specs = [
        ('rgb16-planar', 2, 2, 1, 1, 1, 'match'),
        ('rgb16-planar-mismatch', 2, 2, 1, 1, 1, 'mismatch'),
        ('rgb16-planar-absent', 2, 2, 1, 1, 1, 'absent'),
        ('rgb16-planar-invalid', 2, 2, 1, 1, 1, 'invalid'),
        ('black16-planar-o2', 1, 2, 2, 1, 1, 'match'),
        ('white16-planar-o4', 0, 2, 4, 1, 1, 'match'),
        ('rgb16-planar-o3', 2, 2, 3, 1, 1, 'match'),
        ('rgb16-planar-packbits-rows2', 2, 2, 1, 32773, 1, 'match'),
        ('rgb16-planar-void', 2, 2, 1, 1, 4, 'match'),
        ('rgb16-control', 2, 1, 1, 1, 1, 'match'),
        ('black16-control', 1, 1, 1, 1, 1, 'match'),
        ('white16-control', 0, 1, 1, 1, 1, 'match'),
        ('rgb16-control-mismatch', 2, 1, 1, 1, 1, 'mismatch'),
        ('black16-control-mismatch', 1, 1, 1, 1, 1, 'mismatch')]
    outputs, records = {}, []
    header = '''/* Generated from literal original integers, without a CMM. */
#if HAVE_LIBTIFF
static struct {
    char const *name;
    int photo, planar, orientation, compression, sample_format, matched;
    uint16_t original[18];
    uint32_t source[18];
} const ts_cases[] = {
'''
    for name, photo, planar, orientation, compression, sample_format, kind in specs:
        samples = (rgb if photo == 2 else
                   [[65535 - p[0] if photo == 0 else p[0]] for p in rgb])
        profile = gray if (photo != 2) != (kind == 'mismatch') else p3
        profile = None if kind == 'absent' else b'not-an-icc' if kind == 'invalid' else profile
        item = dict(name=name, photo=photo, planar=planar,
                    orientation=orientation, compression=compression,
                    sample_format=sample_format, profile=kind, samples=samples)
        words = source_words(item)
        data = literal(item, profile)
        filename = 'tiff-separate16-' + name + '.tif'
        outputs[filename] = data
        records.append(dict(item, file=filename, sha256=hashlib.sha256(data).hexdigest(),
                            profile_sha256=hashlib.sha256(profile).hexdigest() if profile else None,
                            source_words=['%08x' % word for word in words]))
        fields = [photo, planar, orientation, compression, sample_format, int(kind == 'match')]
        header += '    { "%s",\n        %s, {\n' % (name, ', '.join(map(str, fields)))
        for values, fmt in [(sum(samples, []), str), (words, lambda v: '0x%08xu' % v)]:
            header += '\n'.join('        ' + ', '.join(fmt(v) for v in values[i:i+4]) + ','
                                for i in range(0, len(values), 4))
            header += '\n    }, {\n' if fmt is str else '\n    } },\n'
    outputs['tiff-separate16.h'] = (header + '};\n#endif\n').encode()
    outputs['tiff-separate16.json'] = (json.dumps(dict(
        generator='tools/generate_tiff_separate16_fixtures.py',
        oracle='Literal source integers and exact rational-to-binary32 ties-to-even; '
               'direct16 output comparisons qualify the adapter, not CMM accuracy.',
        fixtures=records), indent=2) + '\n').encode()
    return outputs
