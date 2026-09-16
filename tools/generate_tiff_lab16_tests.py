#!/usr/bin/env python3
"""Literal signed TIFF CIELAB16 words and independent public-frame values."""

from decimal import Decimal as D, localcontext
import hashlib
import json
import struct

from generate_colorsync_lab_tests import bounds
from generate_tiff_lab_linear_tests import linear_bounds, linear_matrices
from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, f32
from generate_tiff_icc_tests import gamma, lab_xyz, read_tiff


def samples():
    # Raw words and physical chroma are fixed separately from the decoder.
    # Neither side is obtained by reversing the implementation under test.
    chroma = [(0x0100, D(1)), (0xff00, D(-1)),
              (0x7fff, D(32767) / 256), (0x8000, D(-128)),
              (0xffff, -D(1) / 256), (0x0001, D(1) / 256)]
    raw = [[32768, 0, 0]]
    physical = [[D(32768) * 100 / 65535, D(0), D(0)]]
    for channel in (1, 2):
        for word, value in chroma:
            words, lab = [32768, 0, 0], [physical[0][0], D(0), D(0)]
            words[channel], lab[channel] = word, value
            raw.append(words)
            physical.append(lab)
    raw += [[0, 0, 0], [65535, 0, 0]]
    physical += [[D(0), D(0), D(0)], [D(100), D(0), D(0)]]
    mid_raw = [[32768, 16384, 0], [32768, 49152, 0],
               [32768, 0, 16384], [32768, 0, 49152]]
    mid_physical = [[physical[0][0], D(a), D(b)]
                    for a, b in [(64, 0), (-64, 0), (0, 64), (0, -64)]]
    return raw, physical, mid_raw, mid_physical


def encode_tiff(raw, endian, profile=b''):
    """Write literal words directly, without a TIFF library or a CMM."""
    width = len(raw)
    entries = {256: (4, [width]), 257: (4, [1]), 258: (3, [16] * 3),
               259: (3, [1]), 262: (3, [8]), 273: (4, [0]),
               277: (3, [3]), 278: (4, [1]), 279: (4, [width * 6]),
               284: (3, [1]), 339: (3, [1] * 3)}
    if profile:
        entries[34675] = (7, profile)
    start = 8 + 2 + len(entries) * 12 + 4
    payload, table = bytearray(), []
    for tag, (kind, value) in sorted(entries.items()):
        blob = (value if kind == 7 else
                struct.pack(endian + {3: 'H', 4: 'I'}[kind] * len(value), *value))
        field = (blob.ljust(4, b'\0') if len(blob) <= 4 else
                 struct.pack(endian + 'I', start + len(payload)))
        if len(blob) > 4:
            payload.extend(blob)
            if len(payload) % 2:
                payload.append(0)
        table.append([tag, struct.pack(endian + 'HHI', tag, kind, len(value)) + field])
    offset = start + len(payload)
    for row in table:
        if row[0] == 273:
            row[1] = row[1][:8] + struct.pack(endian + 'I', offset)
    prefix = b'II' if endian == '<' else b'MM'
    words = struct.pack(endian + 'H' * (width * 3), *[v for row in raw for v in row])
    return (prefix + struct.pack(endian + 'HIH', 42, 8, len(entries))
            + b''.join(row[1] for row in table) + bytes(4) + payload + words)


def build(linear_profile):
    with localcontext() as context:
        context.prec = 90
        return build_values(linear_profile)


def build_values(linear_profile):
    raw, physical, mid_raw, mid_physical = samples()
    # The existing identity Lab profile is only input data. Expectations use
    # independent physical Lab formulas, never production/native CMS output.
    profile = read_tiff('lab_high_precision_icc.tiff', True)['profile']
    assert hashlib.sha256(profile).hexdigest() == (
        '42831b7c1a5fb25d738bd5c09c67343363f897f63053f808f2b3315fc4ef8215')
    matrices = linear_matrices(linear_profile)
    fallback = [[D(v) for v in row] for row in
                [['3.2404542', '-1.5371385', '-.4985314'],
                 ['-.9692660', '1.8760108', '.0415560'],
                 ['.0556434', '-.2040259', '1.0572252']]]

    def clip(value):
        return max(D(0), min(D(1), value))

    def units(lab):
        return [f32(v / (100 if c == 0 else 128)) for c, v in enumerate(lab)]

    def xyz(lab):
        return lab_xyz([D.from_float(v) * (100 if c == 0 else 128)
                        for c, v in enumerate(units(lab))])

    def fallback_bounds(lab):
        xyz65 = [v * white / pcs for v, white, pcs in zip(xyz(lab),
                 [D('.95047'), D(1), D('1.08883')],
                 [D('.9642'), D(1), D('.8249')])]
        return bounds([[gamma(clip(v)) for v in apply(fallback, xyz65)]])

    limits = [[], [], [], []]
    for lab in physical:
        limits[0].append(fallback_bounds(lab))
        for engine, interval in enumerate(linear_bounds(units(lab), matrices), 1):
            limits[engine].append(interval)
    mid_limits = [fallback_bounds(lab) for lab in mid_physical]
    icc_indices = [0, 1, 2, 5, 6, 7, 8, 11, 12, 13, 14]
    header = ('/* Offline physical Lab formulas; no CMM-derived expected values. */\n'
              + array('lab16_bounds', 'double', limits)
              + 'static int const lab16_icc_check[15] = {\n    '
              + ', '.join(str(int(i in icc_indices)) for i in range(15))
              + '\n};\n')
    outputs = {'tiff-lab16-values.h': header.encode(),
               'tiff-lab16-mid-values.h': (
                   '/* Supplemental independent D65 mid-chroma values. */\n'
                   + array('lab16_mid_bounds', 'double', mid_limits)).encode()}
    for endian, label in [('<', 'le'), ('>', 'be')]:
        for name, icc in [('untagged', b''), ('icc', profile)]:
            outputs[f'tiff-lab16-{label}-{name}.tif'] = encode_tiff(raw, endian, icc)
        outputs[f'tiff-lab16-{label}-mid.tif'] = encode_tiff(mid_raw, endian)
    record = dict(generator='tools/generate_tiff_lab16_tests.py',
        decimal_digits=90, raw=raw, physical=physical, normalized=list(map(units, physical)),
        midpoint_raw=mid_raw, midpoint_physical=mid_physical,
        modes=['untagged D65 gamma', 'builtin direct linear',
               'LCMS direct linear', 'ColorSync direct linear'],
        bounds=limits, midpoint_bounds=mid_limits, icc_indices=icc_indices,
        contract='Photometric8 CIELAB16: L unsigned /65535; twos-complement a/b physical signed/256, facade signed/32768; uint16 containers preserve raw words.',
        independence='Literal words and physical values are specified together; expected RGB uses physical Lab formulas, not a decoder/CMM/reversed encoder.',
        budgets='Existing 64u32 path-specific intervals; LCMS one encoded Lab bin; ColorSync canonical/stored/D50-row-normalized linear profile interval. No fitted tolerance.',
        profile_sha256=hashlib.sha256(profile).hexdigest(),
        fixtures_sha256={n: hashlib.sha256(data).hexdigest() for n, data in outputs.items()
                        if n.endswith('.tif')})
    outputs['tiff-lab16.json'] = (json.dumps(record, indent=2, default=str) + '\n').encode()
    return outputs
