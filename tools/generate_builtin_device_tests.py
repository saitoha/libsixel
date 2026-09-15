#!/usr/bin/env python3
"""Independent builtin CMYK float / gray byte expectations; no CMM calls."""

from decimal import Decimal as D, localcontext
import hashlib
import itertools
import json
import struct

from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, destination_matrix, f32, inverse, tags


def gamma(value):
    # Builtin analytic sRGB extends by signed magnitude. Native CMM curve
    # extensions and their fixed-point budgets do not define this endpoint.
    magnitude = abs(value)
    encoded = (magnitude * D('12.92') if magnitude <= D('.0031308') else
               D('1.055') * magnitude ** (1 / D('2.4')) - D('.055'))
    return -encoded if value < 0 else encoded


def build(cmyk_profile, cmyk_record, gray_profile):
    with localcontext() as context:
        context.prec = 90
        return build_values(cmyk_profile, json.loads(cmyk_record), gray_profile)


def build_values(cmyk_profile, cmyk_record, gray_profile):
    base = cmyk_record['base_u1_15']
    weights = cmyk_record['weights_u1_15']
    fields = tags(cmyk_profile)
    lut = fields[b'A2B1']
    vertices = [[base[j] - sum(bit * weights[k][j]
                             for k, bit in enumerate(bits)) for j in range(3)]
                for bits in itertools.product((0, 1), repeat=4)]
    # Read back every CLUT vertex and both ramps. The affine source model
    # is exact for this stored ICC; no native quantization margin is needed.
    assert cmyk_profile[16:24] == b'CMYKXYZ '
    assert fields[b'A2B0'] == lut
    assert lut[:4] == b'mft2' and lut[8:11] == bytes([4, 3, 2])
    assert struct.unpack_from('>HH', lut, 48) == (2, 2)
    assert lut[52:68] == struct.pack('>HH', 0, 65535) * 4
    assert lut[68:164] == b''.join(struct.pack('>3H', *v) for v in vertices)
    assert lut[164:] == struct.pack('>HH', 0, 65535) * 3
    assert gray_profile[16:24] == b'GRAYXYZ '
    assert tags(gray_profile)[b'kTRC'] == (
        b'para' + bytes(8) + struct.pack('>i', 2 * 65536))
    for profile in [cmyk_profile, gray_profile]:
        assert struct.unpack_from('>I', profile)[0] == len(profile)
        assert profile[36:40] == b'acsp'
    cmyk_samples = [[f32(D(v) / 65535) for v in row]
                    for row in cmyk_record['source16']]
    cmyk_samples += [[0, 1, 1, 0], [1, 0, 1, 0], [1, 1, 0, 0],
                    [0, 0, 0, 0], [1, 1, 1, 1], [.25, .5, .75, .125],
                    [.5, .5, .5, 0], [1, 0, 0, 1], [1 - 2**-16] * 3 + [0]]
    assert len(cmyk_samples) == 17
    matrix = inverse(destination_matrix())
    datasets = {}
    for kind, samples in [('cmyk', cmyk_samples),
                          ('gray', [[v] for v in range(256)])]:
        expected = []
        for sample in samples:
            if kind == 'gray':
                # A gray curve scales PCS D50; neutral destination RGB is
                # the same value in all channels, independently of a CMM.
                value = D(sample[0]) / 255
                linear = [value * value] * 3
            else:
                inks = [D.from_float(v) for v in sample]
                xyz = [(D(base[j]) - sum(inks[k] * weights[k][j]
                                        for k in range(4))) / 32768
                       for j in range(3)]
                linear = apply(matrix, xyz)
            expected.append([linear, [gamma(v) for v in linear]])
        datasets[kind] = dict(samples=samples, expected=expected)
    header = '/* Generated independently from affine ICC and gray gamma 2. */\n'
    header += array('bd_cmyk_source', 'float', cmyk_samples)
    header += array('bd_cmyk_expected', 'double', datasets['cmyk']['expected'])
    header += array('bd_gray_expected', 'double', datasets['gray']['expected'])
    record = dict(precision_decimal=90, datasets=datasets,
        source_profile_sha256={name: hashlib.sha256(blob).hexdigest()
                               for name, blob in [('cmyk.icc', cmyk_profile),
                                  ('gray-gamma2.icc', gray_profile)]},
        model='Affine CMYK u1.15 CLUT to independently derived D50 sRGB; gray (v/255)^2; signed-magnitude analytic sRGB gamma.',
        float_margin='max(abs(expected RGB triplet)) * 64 / 2^24; black exactly zero; no absolute floor or native CMM margin')
    return {'builtin-device.h': header.encode(),
            'builtin-device.json': (json.dumps(record, indent=2, default=str)
                                    + '\n').encode()}
