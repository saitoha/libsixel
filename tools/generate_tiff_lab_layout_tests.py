#!/usr/bin/env python3
"""Literal Lab TIFF layouts; physical Lab and independent RGB value oracles.

ICCLAB8: ICC.1:2004-10 Lab encoding. ITULAB8: RFC 3949 section 6.2.3.
CIELAB16: the signed-word contract in docs/loader/color-management/README.md.
No TIFF decoder, production color function or CMM generates expectations.
"""
from decimal import Decimal as D, localcontext
import hashlib
import json
import struct

from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, f32
from generate_tiff_icc_tests import gamma, lab_xyz, read_tiff
from generate_tiff_lab16_tests import samples
from generate_tiff_lab_linear_tests import linear_bounds, linear_matrices


def directory(tags, endian):
    start = 8 + 2 + len(tags) * 12 + 4
    entries, extra = [], bytearray()
    for tag, (kind, values) in sorted(tags.items()):
        blob = values if kind == 7 else struct.pack(
            endian + {3: 'H', 4: 'I', 10: 'i'}[kind] * len(values), *values)
        count = len(values) // 2 if kind == 10 else len(values)
        if len(blob) <= 4:
            field = blob.ljust(4, b'\0')
        else:
            extra.extend(bytes(-len(extra) % 2))
            field = struct.pack(endian + 'I', start + len(extra))
            extra.extend(blob)
        entries.append(struct.pack(endian + 'HHI', tag, kind, count) + field)
    return ((b'II' if endian == '<' else b'MM') +
            struct.pack(endian + 'HIH', 42, 8, len(tags)) +
            b''.join(entries) + bytes(4) + extra)


def fixture(raw, photo, endian, orientation, packed, profile, decode):
    bits, separate = (16, True) if photo == 8 else (8, False)
    tags = {256: (4, [5]), 257: (4, [3]), 258: (3, [bits] * 3),
            259: (3, [32773 if packed else 1]), 262: (3, [photo]),
            274: (3, [orientation]), 277: (3, [3]), 278: (4, [2]),
            284: (3, [2 if separate else 1]), 339: (3, [1] * 3),
            34675: (7, profile)}
    if decode:
        tags[433] = (10, [v for n in decode for v in (n, 1)])
    parts = []
    for plane in range(3 if separate else 1):
        for y in (0, 2):
            part = bytearray()
            for row in range(y, min(y + 2, 3)):
                values = [v for pixel in raw[row * 5:(row + 1) * 5]
                          for v in ([pixel[plane]] if separate else pixel)]
                blob = struct.pack(endian + ('H' if bits == 16 else 'B') *
                                   len(values), *values)
                part.extend(bytes([len(blob) - 1]) + blob if packed else blob)
            parts.append(bytes(part))
    tags[273] = (4, [0] * len(parts))
    tags[279] = (4, list(map(len, parts)))
    offset = len(directory(tags, endian))
    for i, part in enumerate(parts):
        tags[273][1][i] = offset
        offset += len(part)
    return directory(tags, endian) + b''.join(parts)


def build(linear_profile):
    with localcontext() as context:
        context.prec = 90
        return build_values(linear_profile)


def build_values(linear_profile):
    raw16, physical16, _, _ = samples()
    raw8 = [[0, 128, 128], [255, 128, 128], [128, 128, 128],
            [129, 129, 127], [130, 127, 129], [180, 180, 160],
            [160, 80, 100], [128, 255, 0], [128, 0, 255],
            [128, 127, 128], [128, 128, 127], [128, 129, 128],
            [128, 128, 129], [254, 128, 128], [1, 128, 128]]
    rawitu = [[r[0], r[1], 96 if r[2] == 128 else r[2]] for r in raw8]
    specs = [('icclab8', 9, '<', 1, False, None),
             ('itulab8', 10, '<', 1, True, None),
             ('itulab8-decode', 10, '>', 3, False, [10, 90, -40, 60, -80, 20]),
             ('lab16-separate-le', 8, '<', 1, False, None),
             ('lab16-separate-be', 8, '>', 2, False, None),
             ('lab16-separate-packbits', 8, '<', 4, True, None)]
    profile = read_tiff('lab_high_precision_icc.tiff', True)['profile']
    matrices = linear_matrices(linear_profile)
    d65_matrix = [[D(v) for v in row] for row in
                  [['3.2404542', '-1.5371385', '-.4985314'],
                   ['-.9692660', '1.8760108', '.0415560'],
                   ['.0556434', '-.2040259', '1.0572252']]]
    header = '/* Generated literal samples and independent Lab/XYZ oracles. */\n'
    entries, record = [], []
    for index, (name, photo, endian, orientation, packed, decode) in enumerate(specs):
        raw = raw16 if photo == 8 else rawitu if photo == 10 else raw8
        physical = []
        for i, row in enumerate(raw):
            if photo == 8:
                physical.append(physical16[i])
            elif decode:
                physical.append([D(decode[c * 2]) + D(row[c]) / 255 *
                                 (decode[c * 2 + 1] - decode[c * 2])
                                 for c in range(3)])
            elif photo == 9:
                physical.append([D(row[0]) * 100 / 255,
                                 D(row[1] - 128), D(row[2] - 128)])
            else:
                physical.append([D(row[0]) * 100 / 255,
                                 D(row[1] - 128) * 170 / 255,
                                 D(row[2] - 96) * 200 / 255])
        ordered = [None] * 15
        for i, lab in enumerate(physical):
            x, y = i % 5, i // 5
            if orientation in (2, 3):
                x = 4 - x
            if orientation in (3, 4):
                y = 2 - y
            ordered[y * 5 + x] = [f32(v / (100 if c == 0 else 128))
                                   for c, v in enumerate(lab)]
        bounds = [linear_bounds(row, matrices) for row in ordered]
        # The builtin identity ICC Lab route has the ICC encoded device
        # domain [-128,127], unlike TIFF CIELAB16's upper endpoint 127+255/256.
        # Source retention is exact; the profile evaluator owns this clamp.
        for row, limits in zip(ordered, bounds):
            clipped = [row[0]] + [min(v, 127 / 128) for v in row[1:]]
            limits[0] = linear_bounds(clipped, matrices)[0]
        fallback = []
        for row in ordered:
            xyz = lab_xyz([D.from_float(v) * (100 if c == 0 else 128)
                           for c, v in enumerate(row)])
            if photo == 8:
                xyz = [v * white / pcs for v, white, pcs in zip(xyz,
                    map(D, ['.95047', '1', '1.08883']),
                    map(D, ['.9642', '1', '.8249']))]
                rgb = [gamma(v)
                       for v in apply(d65_matrix, xyz)]
            else:
                rgb = apply(matrices[0], xyz)
            fallback.append(rgb)
        blob = fixture(raw, photo, endian, orientation, packed, profile, decode)
        symbol = f'tlx_data_{index}'
        header += f'static unsigned char const {symbol}[] = {{\n'
        for start in range(0, len(blob), 12):
            header += '    ' + ', '.join(f'0x{b:02x}' for b in blob[start:start+12]) + ',\n'
        header += '};\n'
        header += array(f'tlx_source_{index}', 'float', ordered)
        header += array(f'tlx_bounds_{index}', 'double', bounds)
        header += array(f'tlx_fallback_{index}', 'double', fallback)
        entries.append(f'    {{{symbol}, sizeof({symbol}), {photo},\n        '
                       f'tlx_source_{index}, tlx_bounds_{index}, tlx_fallback_{index}}},\n')
        record.append(dict(name=name, raw=raw, physical=physical, source=ordered,
                           bounds=bounds, fallback=fallback,
                           sha256=hashlib.sha256(blob).hexdigest()))
    header += ('static struct {\n    unsigned char const *bytes;\n'
               '    size_t size;\n    int photo;\n'
               '    float const (*source)[3];\n'
               '    double const (*bounds)[3][2][3];\n'
               '    double const (*fallback)[3];\n'
               '} const tlx_cases[] = {\n' + ''.join(entries) + '};\n')
    manifest = dict(generator='tools/generate_tiff_lab_layout_tests.py',
        profile_sha256=hashlib.sha256(profile).hexdigest(), cases=record,
        independence='Literal raw samples; Decimal Lab/XYZ/matrix equations. No live CMM oracle.',
        budget='Existing 64u32 builtin, one encoded Lab bin LCMS, canonical/stored matrix ColorSync intervals. Builtin ICC device chroma is bounded at 127. Fallback relative 64u32 per triplet; exact black, no absolute dark floor.')
    return {'tiff-lab-layout.h': header.encode(),
            'tiff-lab-layout.json': (json.dumps(manifest, indent=2, default=str) + '\n').encode()}
