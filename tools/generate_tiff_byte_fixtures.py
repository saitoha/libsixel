#!/usr/bin/env python3
"""Literal byte TIFFs; rational normalization and independent ICC tag oracle."""
import hashlib
import json
import struct
from fractions import Fraction as F
import tiff_managed_reference as ref


def write_tiff(pixels, photo, icc, orientation=1, width=None, sample=1):
    """Write original samples directly, without a TIFF or CMS dependency."""
    channels = len(pixels[0])
    raw = bytes(sum(pixels, []))
    width = width or len(pixels)
    height = len(pixels) // width
    tags = {256: (4, [width]), 257: (4, [height]),
            258: (3, [8] * channels), 259: (3, [1]), 262: (3, [photo]),
            273: (4, [0]), 274: (3, [orientation]), 277: (3, [channels]),
            278: (4, [height]), 279: (4, [len(raw)]), 284: (3, [1]),
            339: (3, [sample] * channels)}
    if icc is not None:
        tags[34675] = (7, icc)
    start = 8 + 2 + len(tags) * 12 + 4
    entries, extra = [], bytearray()
    for tag, (kind, values) in sorted(tags.items()):
        packed = values if kind == 7 else struct.pack(
            '<' + ('H' if kind == 3 else 'I') * len(values), *values)
        if len(packed) <= 4:
            field = packed.ljust(4, b'\0')
        else:
            extra.extend(bytes(-len(extra) % 2))
            field = struct.pack('<I', start + len(extra))
            extra.extend(packed)
        entries.append([tag, kind, len(values), field])
    for entry in entries:
        if entry[0] == 273:
            entry[3] = struct.pack('<I', start + len(extra))
    return (b'II*\0' + struct.pack('<IH', 8, len(entries)) +
            b''.join(struct.pack('<HHI', t, k, n) + v
                     for t, k, n, v in entries) + bytes(4) + extra + raw)


def build(p3, gray):
    outputs, records, cases = {}, [], []
    rgb = [[255, 0, 0], [0, 255, 0], [0, 0, 255], [128, 64, 32],
           [1, 2, 3], [255, 255, 255]]
    grays = [[v] for v in [0, 1, 2, 26, 128, 255]]
    for name, photo, pixels, profile in [('rgb', 2, rgb, p3),
                                        ('black', 1, grays, gray),
                                        ('white', 0, grays, gray),
                                        ('bounded', 2,
                                         [[128, 64, 32]]*6, p3)]:
        words = [ref.nearest_rational(F(v if photo else 255-v, 255))
                 for row in pixels for v in (row if photo == 2 else row*3)]
        source = [struct.unpack('>f', struct.pack('>I', w))[0] for w in words]
        expected = ref.expected(dict(icc=profile,
            triples=[source[i:i+3] for i in range(0, 18, 3)]))
        # Encode from the independent canonical oracle, never from the loader.
        gamma = [float(F(323, 25)*F(x)) if abs(x) <= 0.0031308 else
                 (1 if x >= 0 else -1) *
                 float(F(211, 200)*ref.power(F(abs(x)), F(5, 12))-
                       F(11, 200)) for x in expected]
        byte = [min(255, max(0, int(max(0, x)*255+0.5))) for x in gamma]
        cases.append(dict(name=name, photo=photo, stored=sum(pixels, []),
                          source=words, expected=expected, gamma=gamma,
                          byte=byte))
        variants = [('icc', pixels, profile, 1, None, 1)]
        for kind, icc in [('none', None), ('invalid', b'not-an-icc'),
                          ('mismatch', gray if photo == 2 else p3),
                          ('icc', profile)]:
            allbytes = ([[v, (73*v+17) % 256, 255-v] for v in range(256)]
                        if photo == 2 else [[v] for v in range(256)])
            variants.append(('all256-'+kind, allbytes, icc, 1, None, 1))
        for orientation in range(1, 5):
            for sample in [1, 4]:
                variants.append((f'o{orientation}-s{sample}', pixels,
                                 profile, orientation, 3, sample))
        for suffix, samples, icc, orientation, width, sample in variants:
            # A bounded fault witness also keeps deliberate bad-publication
            # mutations within the defined float-to-byte conversion domain.
            if name == 'bounded' and suffix != 'icc':
                continue
            filename = f'tiff-byte-{name}-{suffix}.tif'
            blob = write_tiff(samples, photo, icc, orientation, width, sample)
            outputs[filename] = blob
            records.append(dict(file=filename,
                sha256=hashlib.sha256(blob).hexdigest()))
    header = '''/* Independent original-byte and ICC-tag oracle. */
#ifndef SIXEL_TIFF_BYTE_VALUES_H
#define SIXEL_TIFF_BYTE_VALUES_H
#include <stdint.h>
static double const tb_float_relative = 0.000003814697265625;
static double const tb_native_relative = 0.00032384445817172027;
static struct {
    char const *name;
    int photo;
    unsigned char stored[18];
    uint32_t source[18];
    double expected[18];
    double gamma[18];
    unsigned char byte[18];
} const tb_cases[] = {
'''
    for case in cases:
        header += '    { "%s", %d, {\n' % (case['name'], case['photo'])
        for field, fmt in [('stored', '%d'), ('source', '0x%08xu'),
                           ('expected', '%.17g'), ('gamma', '%.17g'),
                           ('byte', '%d')]:
            header += ',\n'.join('        ' + fmt % v for v in case[field])
            header += '\n    }' + (', {\n' if field != 'byte' else ' },\n')
    header += '};\n#endif\n'
    outputs['tiff-byte.h'] = header.encode()
    outputs['tiff-byte.json'] = (json.dumps(dict(
        generator='tools/generate_tiff_byte_fixtures.py',
        oracle='Rational binary32 normalization; independent ICC tag reader, '
               'rational matrices/Bradford and Decimal60 power; no CMM.',
        allowance='Existing declared relative test budgets; exact zero black; '
                  'not a proof of opaque native arithmetic.',
        cases=cases, fixtures=records), indent=2)+'\n').encode()
    return outputs
