#!/usr/bin/env python3
"""Independent Gray8 formatter values and fixed ICC targets; no CMM calls."""

from decimal import Decimal as D, localcontext
import hashlib
import json
import struct

from generate_colorsync_lab_tests import byte_array, encode_fixed, normalize, read_matrix
from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, inverse, tags
from generate_tiff_icc_tests import gamma


def build(gray_profile, linear_profile, gamma_profile, pack_profile, fixed):
    with localcontext() as context:
        context.prec = 90
        return build_values(gray_profile, linear_profile, gamma_profile,
                            pack_profile, fixed)


def build_values(gray_profile, linear_profile, gamma_profile, pack_profile, fixed):
    identity = b'para' + bytes(8) + fixed(D(1))
    identity_profile = pack_profile([(b'kTRC', identity)], b'scnr', b'GRAY',
        'Independent Gray identity for source-format checks')
    assert tags(gray_profile)[b'kTRC'] == b'para' + bytes(8) + fixed(D(2))
    for profile in [gray_profile, identity_profile]:
        assert profile[16:24] == b'GRAYXYZ '
        assert profile[36:40] == b'acsp'
        assert struct.unpack_from('>I', profile)[0] == len(profile)
    fields = tags(gamma_profile)
    curve = fields[b'rTRC']
    assert curve[:4] == b'para' and int.from_bytes(curve[8:10], 'big') == 4
    assert fields[b'gTRC'] == fields[b'bTRC'] == curve
    linear_fields = tags(linear_profile)
    assert all(linear_fields[k] == identity for k in [b'rTRC', b'gTRC', b'bTRC'])
    assert all(fields[k] == linear_fields[k]
               for k in [b'rXYZ', b'gXYZ', b'bXYZ', b'wtpt', b'chad'])
    assert fields[b'wtpt'][8:] == b''.join(map(fixed, [D('.9642'), D(1), D('.8249')]))
    parameters = [D(x) / 65536 for x in struct.unpack_from('>7i', curve, 12)]
    matrix = read_matrix(linear_profile)
    matrices = [inverse(matrix), inverse(normalize(matrix))]
    datasets = {}
    header = '/* Independent Gray8 normalization and fixed-target intervals. */\n'
    for name, samples in [('gamma2', [26, 253]), ('identity', list(range(256)))]:
        ideals, intervals = [], []
        for sample in samples:
            # Keep the original gamma2 positive pair separate from the full
            # identity formatter grid. Native raw power-curve dark behavior
            # is a different boundary, never a widened numeric allowance.
            y = (D(sample) / 255) ** (2 if name == 'gamma2' else 1)
            xyz = [D('.9642') * y, y, D('.8249') * y]
            linear = [[y] * 3] + [apply(m, xyz) for m in matrices]
            encoded = [[gamma(y)] * 3] + [
                [encode_fixed(x, parameters) for x in row] for row in linear[1:]]
            bounds = []
            for models in [linear, encoded]:
                # Relative triplet margin includes float source rounding;
                # exact black stays zero, with no absolute error floor.
                margin = max(abs(x) for row in models for x in row) * D(64) / 2**24
                bounds.append([[min(row[c] for row in models) - margin for c in range(3)],
                               [max(row[c] for row in models) + margin for c in range(3)]])
            ideals.append([[y] * 3, [gamma(y)] * 3])
            intervals.append(bounds)
        datasets[name] = dict(samples=samples, ideal=ideals, bounds=intervals)
        header += array('cg_' + name + '_bounds', 'double', intervals)
    header += array('cg_gamma2_source', 'unsigned char', [26, 253])
    profiles = dict(gamma2=gray_profile, identity=identity_profile,
                    linear=linear_profile, gamma=gamma_profile)
    for name, profile in profiles.items():
        header += byte_array('cg_' + name + '_icc', profile)
    record = dict(precision_decimal=90, datasets=datasets,
        profile_sha256={k: hashlib.sha256(v).hexdigest() for k, v in profiles.items()},
        model='Ideal D50 neutral Gray; inverse stored and row-normalized target matrix; ideal signed sRGB and stored parametric inverse.',
        float_margin='Relative triplet 64/2^24, exact black, no absolute floor; includes byte-to-float source normalization.',
        boundary='Original gamma2 byte26/253 pair; additional identity Gray full-byte formatter control; not raw gamma2 dark-curve conformance.')
    return {'colorsync-gray8.h': header.encode(),
            'colorsync-gray8.json': (json.dumps(record, indent=2, default=str) + '\n').encode(),
            'colorsync-gray-identity.icc': identity_profile}
