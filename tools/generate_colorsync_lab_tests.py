#!/usr/bin/env python3
"""Fixed Lab targets and independent values, without a CMS/factory oracle."""

from decimal import Decimal as D, localcontext
import hashlib
import json
import struct

from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, destination_matrix, f32, inverse, tags
from generate_tiff_icc_tests import gamma, lab_xyz, read_tiff


def normalize(matrix):
    return [[v / sum(row) * white for v in row]
            for row, white in zip(matrix, [D('.9642'), D(1), D('.8249')])]


def read_matrix(profile):
    fields = tags(profile)
    return list(map(list, zip(*[[D(v) / 65536 for v in struct.unpack_from(
        '>3i', fields[key], 8)] for key in [b'rXYZ', b'gXYZ', b'bXYZ']])))


def byte_array(name, data):
    return ('static unsigned char const ' + name + '[] = {\n'
            + ''.join('    ' + ', '.join(f'0x{x:02x}' for x in data[i:i + 12])
                      + ',\n' for i in range(0, len(data), 12)) + '};\n')


def encode_fixed(value, parameters):
    """Signed inverse of the fixed sRGB parametric curve, including its toe."""
    g, a, b, c, d, e, f = parameters
    sign = -1 if value < 0 else 1
    value = abs(value)
    return sign * (((value - e) ** (1 / g) - b) / a
                   if value >= c * d + f else (value - f) / c)


def bounds(models):
    # Endpoints are independently defined ICC/ideal values. The float margin
    # is scoped to these paths, not fitted to the maximum native observation.
    scale = max(abs(v) for model in models for v in model)
    margin = max(D(1), scale) * D(64) / 2**24
    return [[min(m[c] for m in models) - margin for c in range(3)],
            [max(m[c] for m in models) + margin for c in range(3)]]


def bounded_decode(value):
    value = max(D(0), min(D(1), value))
    return (value / D('12.92') if value <= D('.04045') else
            ((value + D('.055')) / D('1.055')) ** D('2.4'))


def build(linear_profile, pack_profile, fixed):
    with localcontext() as context:
        context.prec = 90
        return build_values(linear_profile, pack_profile, fixed)


def build_values(linear_profile, pack_profile, fixed):
    original = read_tiff('lab_high_precision_icc.tiff', True)
    lut = tags(original['profile'])[b'A2B0']
    assert lut[:4] == b'mft1' and lut[8:11] == bytes([3, 3, 2])
    assert lut[48:816] == lut[840:] == bytes(range(256)) * 3
    assert lut[816:840] == bytes(v for a in (0, 255) for b in (0, 255)
                               for c in (0, 255) for v in (a, b, c))
    fields = tags(linear_profile)
    matrix = read_matrix(linear_profile)
    canonical = destination_matrix()
    # The linear fixture is independently generated from public sRGB
    # chromaticities. Verify columns before consuming its serialized fields.
    assert all(matrix[r][c] == D(struct.unpack('>i', fixed(canonical[r][c]))[0])
               / 65536 for r in range(3) for c in range(3))
    identity = b'para' + bytes(4) + struct.pack('>HH', 0, 0) + fixed(D(1))
    assert all(fields[k] == identity for k in [b'rTRC', b'gTRC', b'bTRC'])
    assert fields[b'wtpt'][8:] == b''.join(map(fixed, [D('.9642'), D(1), D('.8249')]))
    parameters = [D('2.4'), 1 / D('1.055'), D('.055') / D('1.055'),
                  1 / D('12.92'), D('.04045'), D(0), D(0)]
    curve = b'para' + bytes(4) + struct.pack('>HH', 4, 0)
    curve += b''.join(map(fixed, parameters))
    gamma_profile = pack_profile([(key, curve if key in [b'rTRC', b'gTRC', b'bTRC']
                                   else value) for key, value in fields.items()
                                  if key not in [b'desc', b'cprt']],
                                 name='Independent sRGB Lab target')
    gamma_fields = tags(gamma_profile)
    for profile in [linear_profile, gamma_profile]:
        assert struct.unpack_from('>I', profile)[0] == len(profile)
        assert profile[16:24] == b'RGB XYZ '
        assert profile[36:40] == b'acsp'
    assert all(gamma_fields[k] == fields[k] for k in [b'rXYZ', b'gXYZ', b'bXYZ', b'wtpt', b'chad'])
    assert all(gamma_fields[k] == curve for k in [b'rTRC', b'gTRC', b'bTRC'])
    stored_parameters = [D(v) / 65536 for v in struct.unpack_from(
        '>7i', gamma_fields[b'rTRC'], 12)]
    matrices = [inverse(canonical), inverse(matrix), inverse(normalize(matrix))]
    samples = [list(row) for row in original['samples']]
    knee_bits = struct.unpack('<I', struct.pack('<f', .08))[0]
    below, knee, above = [struct.unpack('<f', struct.pack('<I', knee_bits + d))[0]
                          for d in [-1, 0, 1]]
    samples += [[0, 0, 0], [1, 0, 0], [below, 0, 0], [knee, 0, 0], [above, 0, 0],
                [.5, 0, 0], [.5, .5, 0], [.5, -.5, 0], [.5, 0, .5], [.5, 0, -.5],
                [.5, .25, -.25], [.5, -.25, .25], [.01, .0625, 0],
                [.99, -.0625, 0], [.25, 0, .125], [.75, 0, -.125]]
    samples = [[f32(v) for v in row] for row in samples]
    assert len(samples) == 272
    sample_bounds, sample_ideal = [], []
    frame_bounds = []
    for i, sample in enumerate(samples):
        physical = [D.from_float(v) * (100 if c == 0 else 128)
                    for c, v in enumerate(sample)]
        xyz = lab_xyz(physical)
        linear_models = [apply(m, xyz) for m in matrices]
        gamma_models = [[gamma(v) for v in linear_models[0]]]
        gamma_models += [[encode_fixed(v, stored_parameters) for v in row]
                         for row in linear_models[1:]]
        sample_bounds.append([bounds(linear_models), bounds(gamma_models)])
        sample_ideal.append([linear_models[0], gamma_models[0]])
        if i < 256:
            frame_bounds.append(bounds(linear_models))
    header = '/* Independent fixed targets and Lab/XYZ/RGB value intervals. */\n'
    header += array('cs_lab_source', 'float', samples)
    header += array('cs_lab_bounds', 'double', sample_bounds)
    header += array('cs_lab_frame_bounds', 'double', frame_bounds)
    header += byte_array('cs_lab_icc', original['profile'])
    header += byte_array('cs_lab_linear_icc', linear_profile)
    header += byte_array('cs_lab_gamma_icc', gamma_profile)
    record = dict(precision_decimal=90, source=samples, ideal=sample_ideal,
        bounds=sample_bounds, frame_bounds=frame_bounds,
        source_profile_sha256=hashlib.sha256(original['profile']).hexdigest(),
        source_tiff_sha256=original['file_sha256'],
        fixed_target_sha256={k: hashlib.sha256(v).hexdigest() for k, v in
                             [('linear', linear_profile), ('gamma', gamma_profile)]},
        fixed_matrix=matrix, fixed_parameters=stored_parameters,
        model='Independent original Lab/XYZ; fixed target ICC matrices/parametric inverse; canonical ideal included. Frame allowance uses the independent fixed linear target models, never live factory output.',
        float_margin='max(1, triplet magnitude) * 64 / 2^24; scoped, not a universal CMM bound')
    return {'colorsync-lab.h': header.encode(),
            'colorsync-lab.json': (json.dumps(record, indent=2, default=str) + '\n').encode(),
            'colorsync-lab-linear.icc': linear_profile,
            'colorsync-lab-gamma.icc': gamma_profile}
