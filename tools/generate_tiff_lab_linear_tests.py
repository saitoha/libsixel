#!/usr/bin/env python3
"""Signed TIFF Lab samples and independent, unclamped linear RGB intervals."""

from decimal import Decimal as D, ROUND_HALF_UP, localcontext
import hashlib
import json
import struct

from generate_colorsync_lab_tests import bounds, read_matrix, normalize
from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, destination_matrix, f32, inverse, tags
from generate_tiff_icc_tests import lab_xyz, read_tiff


def linear_matrices(profile):
    canonical = destination_matrix()
    stored = read_matrix(profile)
    fields = tags(profile)
    identity = b'para' + bytes(8) + struct.pack('>i', 65536)
    assert all(fields[key] == identity for key in (b'rTRC', b'gTRC', b'bTRC'))
    assert all(stored[r][c] == (canonical[r][c] * 65536).to_integral_value() / 65536
               for r in range(3) for c in range(3))
    return [inverse(m) for m in (canonical, stored, normalize(stored))]


def linear_bounds(source, matrices):
    """Propagate declared models/budgets; never consume measured CMS output."""
    physical = [D.from_float(float(v)) * (100 if c == 0 else 128)
                for c, v in enumerate(source)]
    xyz = lab_xyz(physical)
    ideal = apply(matrices[0], xyz)
    margin = max(map(abs, ideal)) * D(64) / 2**24
    builtin = [[v - margin for v in ideal], [v + margin for v in ideal]]
    lightness, a, b = physical
    dl, dab = D(100) / 65535, D(255) / 65535
    xyz_lo = lab_xyz([lightness - dl, a - dab, b + dab])
    xyz_hi = lab_xyz([lightness + dl, a + dab, b - dab])
    lo = [sum(v * (xyz_lo[j] if v >= 0 else xyz_hi[j])
              for j, v in enumerate(row)) - margin for row in matrices[0]]
    hi = [sum(v * (xyz_hi[j] if v >= 0 else xyz_lo[j])
              for j, v in enumerate(row)) + margin for row in matrices[0]]
    colorsync = bounds([apply(matrix, xyz) for matrix in matrices])
    return [builtin, [lo, hi], colorsync]


def encode(samples, floating, profile):
    """Serialize prescribed samples directly, without a TIFF decoder or CMM."""
    count = len(samples)
    raster = struct.pack('<' + ('f' if floating else 'H') * (count * 3),
                         *[v for row in samples for v in row])
    entries = {256: (4, [count]), 257: (4, [1]),
               258: (3, [32 if floating else 16] * 3), 259: (3, [1]),
               262: (3, [8]), 273: (4, [0]), 277: (3, [3]),
               278: (4, [1]), 279: (4, [len(raster)]), 284: (3, [1]),
               339: (3, [3 if floating else 1] * 3), 34675: (7, profile)}
    start = 8 + 2 + len(entries) * 12 + 4
    payload, table = bytearray(), []
    for tag, (kind, value) in sorted(entries.items()):
        blob = (value if kind == 7 else
                struct.pack('<' + {3: 'H', 4: 'I'}[kind] * len(value), *value))
        field = (blob.ljust(4, b'\0') if len(blob) <= 4 else
                 struct.pack('<I', start + len(payload)))
        if len(blob) > 4:
            payload.extend(blob)
            if len(payload) % 2:
                payload.append(0)
        table.append([tag, struct.pack('<HHI', tag, kind, len(value)) + field])
    for row in table:
        if row[0] == 273:
            row[1] = row[1][:8] + struct.pack('<I', start + len(payload))
    return (b'II*\0\x08\0\0\0' + struct.pack('<H', len(entries))
            + b''.join(row[1] for row in table) + bytes(4) + payload + raster)


def build(colorsync_record, linear_profile):
    with localcontext() as context:
        context.prec = 90
        return build_values(json.loads(colorsync_record), linear_profile)


def build_values(record, linear_profile):
    matrices = linear_matrices(linear_profile)
    rows = [record['source'][(i + i // 256 * 37) % 272] for i in range(521)]
    signed = [[int((D.from_float(float(v)) * (65535 if c == 0 else 32768))
                   .to_integral_value(rounding=ROUND_HALF_UP))
               for c, v in enumerate(row)] for row in rows]
    assert all(0 <= row[0] <= 65535 and all(-32768 <= v <= 32767 for v in row[1:])
               for row in signed)
    raw = [[v & 65535 for v in row] for row in signed]
    normalized = [[f32(D(v) / (65535 if c == 0 else 32768))
                   for c, v in enumerate(row)] for row in signed]
    profile = read_tiff('lab_high_precision_icc.tiff', True)['profile']
    assert hashlib.sha256(profile).hexdigest() == record['source_profile_sha256']
    source = [rows, normalized]
    limits = [[linear_bounds(row, matrices) for row in samples] for samples in source]
    header = ('/* Independent physical Lab/XYZ and unclamped linear intervals. */\n'
              + array('tll_source', 'float', source)
              + array('tll_bounds', 'double', limits))
    outputs = {'tiff-lab-linear.h': header.encode()}
    for name, values, floating in [('float', rows, True), ('word', raw, False)]:
        data = encode(values, floating, profile)
        if not floating:
            # Verify the actual serialized strip separately with signed Hhh.
            actual = list(struct.iter_unpack('<Hhh', data[-521 * 6:]))
            assert actual == list(map(tuple, signed))
        outputs[f'tiff-lab-{name}-521.tif'] = data
    manifest = dict(generator='tools/generate_tiff_lab_linear_tests.py',
        precision_decimal=90, source=source, prescribed_signed=signed, raw=raw,
        physical_word=[[D(row[0]) * 100 / 65535, D(row[1]) / 256,
                        D(row[2]) / 256] for row in signed],
        sequence='source[(i + floor(i/256)*37) % 272]',
        rounding='nearest; half away from zero', bounds=limits,
        source_profile_sha256=hashlib.sha256(profile).hexdigest(),
        fixed_linear_profile_sha256=hashlib.sha256(linear_profile).hexdigest(),
        budgets='Unchanged 64u32 path-specific intervals; LCMS one encoded Lab bin; ColorSync canonical/stored/D50-row-normalized fixed linear matrix. No fitted tolerance or gamma/range clamp.',
        independence='Prescribed storage/physical values and independent Lab/XYZ formulas; no production decoder, measured CMS output or live factory oracle.',
        fixtures_sha256={n: hashlib.sha256(v).hexdigest() for n, v in outputs.items()
                        if n.endswith('.tif')})
    outputs['tiff-lab-linear.json'] = (json.dumps(manifest, indent=2, default=str) + '\n').encode()
    return outputs
