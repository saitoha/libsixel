#!/usr/bin/env python3
"""Generate independent color fixtures; never import or execute libsixel.

Chromaticities: ICC Display P3 registry and W3C CSS Color 4.
Matrices are solved with 60-digit Decimal arithmetic rather than copied from
production. Bradford uses the published cone matrix. PNG/ICC bytes are built
here so the reference does not depend on the host's image or CMS libraries.
"""

import argparse
from decimal import Decimal as D, getcontext
import hashlib
import itertools
import json
import math
from pathlib import Path
import struct
import zlib

getcontext().prec = 60
ROOT = Path(__file__).resolve().parents[1]


def matrix(rows):
    return [[D(str(v)) for v in row] for row in rows]


def apply(m, v):
    return [sum(a * b for a, b in zip(row, v)) for row in m]


def inverse(m):
    rows = [row[:] + [D(i == j) for j in range(3)]
            for i, row in enumerate(m)]
    for col in range(3):
        pivot = max(range(col, 3), key=lambda i: abs(rows[i][col]))
        rows[col], rows[pivot] = rows[pivot], rows[col]
        scale = rows[col][col]
        rows[col] = [v / scale for v in rows[col]]
        for i in range(3):
            if i != col:
                scale = rows[i][col]
                rows[i] = [a - scale * b
                           for a, b in zip(rows[i], rows[col])]
    return [row[3:] for row in rows]


def white(x, y):
    x, y = D(x), D(y)
    return [x / y, D(1), (1 - x - y) / y]


def rgb_matrix(primaries):
    columns = [white(x, y) for x, y in primaries]
    unscaled = [list(row) for row in zip(*columns)]
    weights = apply(inverse(unscaled), D65)
    return [[value * weights[j] for j, value in enumerate(row)]
            for row in unscaled]


def decode(value):
    x = abs(value)
    y = x / D('12.92') if x <= D('0.04045') else (
        (x + D('0.055')) / D('1.055')) ** D('2.4')
    return -y if value < 0 else y


def encode(value):
    x = abs(value)
    y = x * D('12.92') if x <= D('0.0031308') else (
        D('1.055') * x ** (1 / D('2.4')) - D('0.055'))
    return -y if value < 0 else y


D65 = white('0.3127', '0.3290')
# ICC PCS illuminant is the encoded D50 tristimulus, not CSS xy D50.
D50 = [D('0.9642'), D(1), D('0.8249')]
SRGB = rgb_matrix([('0.64', '0.33'), ('0.30', '0.60'), ('0.15', '0.06')])
P3 = rgb_matrix([('0.68', '0.32'), ('0.265', '0.69'), ('0.15', '0.06')])
BRADFORD = matrix([['0.8951', '0.2664', '-0.1614'],
                   ['-0.7502', '1.7135', '0.0367'],
                   ['0.0389', '-0.0685', '1.0296']])


def adapt(xyz):
    src, dst = apply(BRADFORD, D65), apply(BRADFORD, D50)
    cone = apply(BRADFORD, xyz)
    return apply(inverse(BRADFORD), [c * d / s
                                    for c, d, s in zip(cone, dst, src)])


def opponents(linear):
    """Independent scalar equations, without production transfer/LUT calls."""
    lms = apply(matrix([
        ['.4122214708', '.5363325363', '.0514459929'],
        ['.2119034982', '.6806995451', '.1073969566'],
        ['.0883024619', '.2817188376', '.6299787005']]), linear)
    roots = [v.copy_sign(D(1)) ** (D(1) / 3) * (-1 if v < 0 else 1)
             for v in lms]
    oklab = apply(matrix([
        ['.2104542553', '.7936177850', '-.0040720468'],
        ['1.9779984951', '-2.4285922050', '.4505937099'],
        ['.0259040371', '.7827717662', '-.8086757660']]), roots)
    # Retain the existing Lab working-space convention independently of
    # the exact chromaticity-derived standard RGB descriptor matrices.
    xyz = apply(matrix([
        ['.4124564', '.3575761', '.1804375'],
        ['.2126729', '.7151522', '.0721750'],
        ['.0193339', '.1191920', '.9503041']]), linear)
    def f(t):
        return t ** (D(1) / 3) if t > D(216) / 24389 else (
            D(24389) / 27 * t + 16) / 116
    def lab_at(x, xn):
        fx, fy, fz = f(x / xn), f(xyz[1]), f(xyz[2] / D('1.08883'))
        return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)]
    lab = lab_at(xyz[0], D('.95047'))
    corrected = lab_at(D('1.12') * xyz[0] - D('.12') * xyz[2],
                       D('1.12') * D('.95047') - D('.12') * D('1.08883'))
    light, a, b = map(float, corrected)
    angle = math.radians(50)
    e = math.cos(angle) * a + math.sin(angle) * b
    f_value = 1.14 * (-math.sin(angle) * a + math.cos(angle) * b)
    chroma = 22.5 * math.log1p(.06 * math.hypot(e, f_value))
    hue = math.atan2(f_value, e) + angle
    din = [325.22 * math.log1p(.0036 * light) / 100,
           chroma * math.cos(hue) / 100, chroma * math.sin(hue) / 100]
    return list(map(float, oklab)), [float(lab[0] / 100),
        float(lab[1] / 128), float(lab[2] / 128)], din


def fixed(value):
    return struct.pack('>i', int((value * 65536).to_integral_value()))


def mluc(text):
    encoded = text.encode('utf-16-be')
    return (b'mluc' + bytes(4) + struct.pack('>II', 1, 12) + b'enUS'
            + struct.pack('>II', len(encoded), 28) + encoded)


def icc_profile(minimal=False):
    tags = []
    for name, column in zip((b'rXYZ', b'gXYZ', b'bXYZ'), zip(*P3)):
        tags.append((name, b'XYZ \0\0\0\0' + b''.join(
            fixed(v) for v in adapt(column))))
    curve = b'para' + bytes(4) + struct.pack('>HH', 4, 0)
    curve += b''.join(fixed(v) for v in [D('2.4'), 1 / D('1.055'),
                     D('0.055') / D('1.055'), 1 / D('12.92'),
                     D('0.04045'), D(0), D(0)])
    tags.extend((name, curve) for name in (b'rTRC', b'gTRC', b'bTRC'))
    tags.append((b'wtpt', b'XYZ ' + bytes(4) + b''.join(map(fixed, D50))))
    if minimal:
        return pack_profile(tags, minimal=True)
    # Matrix/TRC tags are already PCS-adapted. Record the actual D65-to-D50
    # adaptation as required for the display's non-D50 adopted white.
    adaptation = list(zip(*(adapt([D(i == j) for i in range(3)])
                             for j in range(3))))
    tags.append((b'chad', b'sf32' + bytes(4) + b''.join(
        fixed(v) for row in adaptation for v in row)))
    return pack_profile(tags, name='Independent Display P3')


def pack_profile(tags, profile_class=b'mntr', space=b'RGB ',
                 name='Independent color fixture', minimal=False):
    # The older minimal envelopes were parser probes. Numeric image oracles
    # must also include the class-independent required ICC v4 tags.
    tags = list(tags)
    supplied = {name for name, _ in tags}
    for tag, payload in [(b'desc', mluc(name)),
                         (b'cprt', mluc('Copyright libsixel contributors; MIT')),
                         (b'wtpt', b'XYZ ' + bytes(4)
                          + b''.join(map(fixed, D50)))]:
        if not minimal and tag not in supplied:
            tags.append((tag, payload))
    header = bytearray(128)
    header[8:12] = bytes([4, 0x30, 0, 0])
    header[12:24] = profile_class + space + b'XYZ '
    header[24:36] = struct.pack('>6H', 2026, 9, 14, 0, 0, 0)
    header[36:40] = b'acsp'
    header[64:68] = struct.pack('>I', 1)
    header[68:80] = b''.join(map(fixed, D50))
    offset = 132 + 12 * len(tags)
    table, payload = b'', b''
    for name, content in tags:
        table += name + struct.pack('>II', offset, len(content))
        padded = content + bytes((-len(content)) % 4)
        payload += padded
        offset += len(padded)
    header[:4] = struct.pack('>I', offset)
    return bytes(header) + struct.pack('>I', len(tags)) + table + payload


def gray_sample_oracles():
    """A D50-referred gray input profile with the exact kTRC y = x**2."""
    curve = b'para' + bytes(4) + struct.pack('>HH', 0, 0) + fixed(D(2))
    profile = pack_profile([(b'kTRC', curve)], b'scnr', b'GRAY',
                           'Independent gray gamma 2')
    samples = [0, 1, 2, 16, 128, 256, 257, 1024, 2048, 4095, 4096, 4097,
               8192, 16384, 32768, 40001, 65534, 65535]
    # A neutral has XYZ = Y*D50. Bound a full s15Fixed16 bin in each
    # white component and each destination-matrix entry, independently of
    # any native output. ||inv(M)|| * (q + 3q) / (1 - ||inv(M)|| * 3q)
    # bounds the RGB gain error. It scales with Y; black gets no floor.
    destination = [list(row) for row in zip(*[
        adapt(column) for column in zip(*SRGB)])]
    inverse_norm = max(sum(map(abs, row)) for row in inverse(destination))
    quantum = 1 / D(65536)
    matrix_budget = inverse_norm * 4 * quantum / (1 - inverse_norm * 3 * quantum)
    # Reserve 64 binary32 unit roundoffs for input normalization, x^g
    # (g <= 4 here), float curve evaluation, matrix arithmetic and storage.
    # This is a test-design allowance, not a proved ColorSync pow error bound.
    float_budget = D(64) / D(2) ** 24
    native_budget = (1 + matrix_budget) * (1 + float_budget) - 1
    expected = [(D(value) / 65535) ** 2 for value in samples]
    header = '/* Generated independently; gray PCS is kTRC times D50. */\n'
    header += f'#define COLOR_GRAY_SAMPLE_COUNT {len(samples)}\n'
    header += ('static double const color_gray_float_relative = '
               + format(float(float_budget), '.17g') + ';\n')
    header += ('static double const color_gray_native_relative = '
               + format(float(native_budget), '.17g') + ';\n')
    header += 'static unsigned char const color_gray_icc[] = {\n'
    for offset in range(0, len(profile), 12):
        header += '    ' + ', '.join(
            f'0x{v:02x}' for v in profile[offset:offset+12]) + ',\n'
    header += '};\nstatic unsigned int const color_gray_source[] = {\n'
    for offset in range(0, len(samples), 8):
        header += '    ' + ', '.join(map(str, samples[offset:offset+8])) + ',\n'
    header += '};\n'
    header += 'static double const color_gray_linear[] = {\n'
    for value in expected:
        header += '    ' + format(float(value), '.17g') + ',\n'
    header += '};\n'
    record = {'generator': 'tools/generate_color_fixtures.py',
        'model': 'D50-referred gray with kTRC y=x**2, no CMM oracle',
        'profile_class': 'scnr', 'input': 'GRAY', 'PCS': 'XYZ',
        'required_tags': ['kTRC', 'desc', 'cprt', 'wtpt'],
        'source': samples, 'peak': 65535,
        'error_contract': {
            'black': 'exact zero in every channel, no absolute floor',
            'positive': 'abs(actual - expected) <= expected * relative_budget',
            'float_relative_budget': float(float_budget),
            'native_relative_budget': float(native_budget),
            'matrix_quantum': str(quantum),
            'destination_inverse_infinity_norm': str(inverse_norm),
            'matrix_gain_budget': str(matrix_budget),
            'matrix_model': 'one fixed-point bin per white/matrix component',
            'float_model': '64 binary32 unit roundoffs; source gamma at most 4',
            'native_composition': '(1 + matrix_budget)*(1 + float_budget) - 1',
            'budget_scope': 'test allowance, not a proven native CMM error bound'},
        'linear_srgb_neutral': [float(value) for value in expected]}
    variants = '#include <stddef.h>\n'
    variants += ('static struct {\n    size_t length;\n'
                 '    unsigned char bytes[512];\n'
                 f'    double linear[{len(samples)}];\n'
                 '} const color_gray_variants[6] = {\n')
    records = []
    for gamma in map(D, ['.5', '1', '1.8', '2', '2.4', '4']):
        encoded = fixed(gamma)
        stored = D(struct.unpack('>i', encoded)[0]) / 65536
        curve = b'para' + bytes(4) + struct.pack('>HH', 0, 0) + encoded
        data = pack_profile([(b'kTRC', curve)], b'scnr', b'GRAY',
                            f'Independent gray gamma {gamma}')
        assert len(data) <= 512
        expected = [(D(value) / 65535) ** stored for value in samples]
        variants += f'    {{ {len(data)}, {{\n'
        for offset in range(0, len(data), 12):
            variants += '        ' + ', '.join(
                f'0x{v:02x}' for v in data[offset:offset+12]) + ',\n'
        variants += '    }, {\n'
        for value in expected:
            variants += '        ' + format(float(value), '.17g') + ',\n'
        variants += '    } },\n'
        records.append({'gamma_requested': str(gamma), 'gamma_stored': str(stored),
            'sha256': hashlib.sha256(data).hexdigest(), 'profile_length': len(data),
            'source': samples, 'linear': [float(value) for value in expected]})
    variants += '};\n'
    return {'gray-gamma2.icc': profile, 'gray-samples.h': header.encode(),
            'gray-samples.json': (json.dumps(record, indent=2)+'\n').encode(),
            'gray-variants.h': variants.encode(),
            'gray-variants.json': (json.dumps(records, indent=2)+'\n').encode()}


def rgb_power_oracles():
    """Exact stored ICC powers/matrices evaluated from original binary32 RGB."""
    neutral = [0, 1, 2, 16, 128, 256, 257, 1024, 2048, 4095, 4096, 4097,
               8192, 16384, 32768, 40001, 65534, 65535]
    samples = [[v, v, v] for v in neutral] + [
        [1, 257, 4096], [4096, 1, 257], [257, 4096, 1],
        [65535, 0, 0], [0, 65535, 0], [0, 0, 65535]]
    source = [[D(struct.unpack('>f', struct.pack('>f', v / 65535))[0])
               for v in pixel] for pixel in samples]
    definitions = []
    for mask in range(8):
        gammas = [D(1) if mask & (1 << c) else D(g)
                  for c, g in enumerate([2, 4, 3])]
        for kind in [0, 3, 4]:
            definitions.append((f'mask{mask}-para{kind}', gammas, kind,
                                SRGB, D65))
    for channel in range(3):
        for value in [65535, 65537]:
            gammas = list(map(D, [[2, 4, 1], [1, 4, 1], [1, 1, 1]][channel]))
            gammas[[2, 0, 1][channel]] = D(value) / 65536
            definitions.append((f'near{channel}-{value}', gammas, 0, SRGB, D65))
    for gamma in ['.5', '1.8', '2.2', '2', '4']:
        definitions.append(('uniform' + gamma, [D(gamma)] * 3, 0, SRGB, D65))
    # A different source white and wide primaries exercise actual adaptation.
    # Solve its matrix independently; do not borrow a generated CMM profile.
    source_white = white('.3457', '.3585')
    basis = list(map(list, zip(*[white(x, y) for x, y in
        [('.708', '.292'), ('.17', '.797'), ('.131', '.046')]])))
    weights = apply(inverse(basis), source_white)
    wide = [[v * weights[c] for c, v in enumerate(row)] for row in basis]
    definitions.append(('wide-white-gamma4', [D(4)] * 3, 0, wide, source_white))
    destination = list(map(list, zip(*(adapt(col) for col in zip(*SRGB)))))
    inv_destination = inverse(destination)
    norm = max(sum(map(abs, row)) for row in inv_destination)
    quantum = 1 / D(65536)
    matrix_budget = norm * 4 * quantum / (1 - norm * 3 * quantum)
    float_budget = D(64) / D(2) ** 24
    budget = (1 + matrix_budget) * (1 + float_budget) - 1
    records = []
    header = ('/* Generated from stored ICC tags; no CMM or libsixel oracle. */\n'
              '#include <stddef.h>\n'
              f'#define COLOR_RGB_POWER_SAMPLES {len(samples)}\n'
              f'#define COLOR_RGB_POWER_PROFILES {len(definitions)}\n'
              'static double const color_rgb_power_relative = '
              + format(float(budget), '.17g') + ';\n'
              'static unsigned int const color_rgb_power_source[][3] = {\n')
    header += ''.join('    { ' + ', '.join(map(str, p)) + ' },\n' for p in samples)
    header += ('};\nstatic struct {\n    char const *name;\n    size_t length;\n'
               '    unsigned char bytes[1024];\n'
               '    double linear[COLOR_RGB_POWER_SAMPLES][3];\n'
               '} const color_rgb_power_profiles[] = {\n')
    for name, gammas, kind, source_matrix, source_white in definitions:
        src_cone, dst_cone = apply(BRADFORD, source_white), apply(BRADFORD, D50)
        def to_pcs(xyz):
            return apply(inverse(BRADFORD), [c * d / s for c, d, s in
                zip(apply(BRADFORD, xyz), dst_cone, src_cone)])
        tags = [(key, b'XYZ ' + bytes(4) + b''.join(map(fixed, to_pcs(col))))
                for key, col in zip([b'rXYZ', b'gXYZ', b'bXYZ'], zip(*source_matrix))]
        for key, gamma in zip([b'rTRC', b'gTRC', b'bTRC'], gammas):
            params = [gamma] + {0: [], 3: [D(1), D(0), D(0), D(0)],
                               4: [D(1), D(0), D(0), D(0), D(0), D(0)]}[kind]
            tags.append((key, b'para' + bytes(4) + struct.pack('>HH', kind, 0)
                         + b''.join(map(fixed, params))))
        chad = list(zip(*(to_pcs([D(i == j) for i in range(3)]) for j in range(3))))
        tags.append((b'chad', b'sf32' + bytes(4) + b''.join(
            fixed(v) for row in chad for v in row)))
        data = pack_profile(tags, name='Independent RGB ' + name)
        assert len(data) <= 1024
        # Read the serialized fields back so expectations include ICC's
        # s15Fixed16 rounding, without depending on production's parser.
        stored = {}
        for offset in range(132, 132 + 12 * struct.unpack_from('>I', data, 128)[0], 12):
            key, start, length = struct.unpack_from('>4sII', data, offset)
            stored[key] = data[start:start + length]
        columns = [[D(v) / 65536 for v in struct.unpack_from('>3i', stored[key], 8)]
                   for key in [b'rXYZ', b'gXYZ', b'bXYZ']]
        pcs_matrix = list(map(list, zip(*columns)))
        stored_gammas = [D(struct.unpack_from('>i', stored[key], 12)[0]) / 65536
                         for key in [b'rTRC', b'gTRC', b'bTRC']]
        linear = [apply(inv_destination, apply(pcs_matrix,
            [v ** g for v, g in zip(pixel, stored_gammas)])) for pixel in source]
        header += f'    {{ "{name}", {len(data)}, {{\n'
        for offset in range(0, len(data), 12):
            header += '        ' + ', '.join(f'0x{v:02x}' for v in data[offset:offset+12]) + ',\n'
        header += '    }, {\n'
        for pixel in linear:
            header += '        {\n' + ''.join(
                '            ' + format(float(v), '.17g') + ',\n' for v in pixel) + '        },\n'
        header += '    } },\n'
        records.append({'name': name, 'kind': kind, 'profile_hex': data.hex(),
            'sha256': hashlib.sha256(data).hexdigest(),
            'gamma_stored': list(map(str, stored_gammas)),
            'matrix_stored': [list(map(str, row)) for row in pcs_matrix],
            'linear': [list(map(str, pixel)) for pixel in linear]})
    header += '};\n'
    record = {'generator': 'tools/generate_color_fixtures.py',
        'source_u16': samples, 'source_f32': [list(map(str, p)) for p in source],
        'model': 'original binary32 powers, stored XYZ, independent inverse sRGB D50',
        'black': 'exact zero; no absolute error floor',
        'positive': 'max channel absolute error / max absolute expected channel',
        'relative_budget': str(budget), 'matrix_budget': str(matrix_budget),
        'float_budget': str(float_budget),
        'budget_scope': 'same gray native allowance; gamma <= 4 and these matrices/samples only',
        'profiles': records}
    exclusions = '#include <stddef.h>\nstatic struct {\n'
    exclusions += ('    size_t length;\n    unsigned char bytes[768];\n'
                   '} const color_rgb_power_exclusions[] = {\n')
    excluded_records = []
    for kind in [0, 3, 4]:
        tags = [(key, b'XYZ ' + bytes(4) + b''.join(map(fixed, adapt(col))))
                for key, col in zip([b'rXYZ', b'gXYZ', b'bXYZ'], zip(*SRGB))]
        if kind == 0:
            # A complete legacy v2 envelope uses desc/text metadata and a
            # one-entry curveType gamma, not a v4 parametricCurveType tag.
            name = b'Independent v2 gamma 2\0'
            tags += [(b'desc', b'desc' + bytes(4) + struct.pack('>I', len(name))
                      + name + bytes(4 + 4 + 2 + 1 + 67)),
                     (b'cprt', b'text' + bytes(4) + b'libsixel; MIT\0')]
            curve = b'curv' + bytes(4) + struct.pack('>IH', 1, 512)
        else:
            params = [D(2), D(1), D(0), D('.125'), D('.125')]
            if kind == 4:
                params += [D(0), D(0)]
            curve = (b'para' + bytes(4) + struct.pack('>HH', kind, 0)
                     + b''.join(map(fixed, params)))
        tags += [(key, curve) for key in [b'rTRC', b'gTRC', b'bTRC']]
        data = bytearray(pack_profile(tags, name=f'Nonpure RGB para{kind}'))
        if kind == 0:
            data[8:12] = bytes([2, 0x40, 0, 0])
        assert len(data) <= 768
        exclusions += f'    {{ {len(data)}, {{\n'
        for offset in range(0, len(data), 12):
            exclusions += '        ' + ', '.join(
                f'0x{v:02x}' for v in data[offset:offset+12]) + ',\n'
        exclusions += '    } },\n'
        excluded_records.append({'name': 'v2-curv' if kind == 0 else f'nonpure-para{kind}',
            'profile_hex': data.hex(), 'sha256': hashlib.sha256(data).hexdigest(),
            'purpose': 'native passthrough identity, not a CMM accuracy oracle'})
    exclusions += '};\n'
    return {'rgb-power.h': header.encode(),
            'rgb-power.json': (json.dumps(record, indent=2) + '\n').encode(),
            'rgb-power-exclusions.h': exclusions.encode(),
            'rgb-power-exclusions.json': (json.dumps(excluded_records, indent=2) + '\n').encode()}


def lut_profile(reverse, minimal=False):
    # Constant 2x2x2 LUTs are intentionally non-invertible. They test
    # directional capability, not a false exact-roundtrip requirement.
    def lut(values):
        head = b'mft2' + bytes(4) + bytes([3, 3, 2, 0])
        head += b''.join(fixed(D(i == j)) for i in range(3) for j in range(3))
        head += struct.pack('>HH', 2, 2)
        ramps = struct.pack('>6H', 0, 65535, 0, 65535, 0, 65535)
        return head + ramps + struct.pack('>3H', *values) * 8 + ramps
    tags = [(b'A2B0', lut([8192, 16384, 24576]))]
    if reverse:
        tags.append((b'B2A0', lut([13107, 26214, 39321])))
    return pack_profile(tags,
        profile_class=b'mntr' if reverse or minimal else b'scnr',
        minimal=minimal)


def chunk(kind, content):
    return (struct.pack('>I', len(content)) + kind + content
            + struct.pack('>I', zlib.crc32(kind + content)))


def fmt(values):
    return ', '.join(format(float(v), '.17g') for v in values)


def png_sample_oracles(profile):
    """Keep source integers, analytic colors and quantized ICC colors apart."""
    outputs = {}
    records = []
    adaptation = [list(row) for row in zip(*[
        adapt([D(i == j) for i in range(3)]) for j in range(3)])]
    icc_matrix = [[D(struct.unpack('>i', fixed(v))[0]) / 65536
                   for v in row] for row in [list(row) for row in zip(*[
                       adapt(column) for column in zip(*P3)])]]
    params = [D(struct.unpack('>i', fixed(v))[0]) / 65536 for v in
              [D('2.4'), 1 / D('1.055'), D('.055') / D('1.055'),
               1 / D('12.92'), D('.04045')]]
    gamma, a, b, c, knee = params
    header = '/* Generated independently from original PNG samples. */\n'
    header += 'static unsigned int const color_png_source[2][8][4] = {\n'
    expected = {'p3_xyz': [], 'p3_linear': [], 'icc_xyz': [],
                'icc_linear': [], 'srgb_linear': [], 'wide_chrm_linear': []}
    for depth in (8, 16):
        peak = 2 ** depth - 1
        samples = ([[255, 0, 0, 255], [0, 255, 0, 128], [0, 0, 255, 0],
                    [128, 128, 128, 255], [13, 77, 219, 64],
                    [255, 255, 255, 255], [0, 0, 0, 255], [103, 151, 197, 193]]
                   if depth == 8 else
                   [[65535, 0, 0, 65535], [0, 65535, 0, 32768],
                    [0, 0, 65535, 0], [32768, 32768, 32768, 65535],
                    [1000, 19001, 60001, 16384], [65535, 65535, 65535, 65535],
                    [0, 0, 0, 65535], [26369, 38903, 50201, 49513]])
        header += '    {\n' + ''.join('        {' + ', '.join(map(str, row))
                                     + '},\n' for row in samples) + '    },\n'
        values = {key: [] for key in expected}
        for sample in samples:
            encoded = [D(v) / peak for v in sample[:3]]
            linear = list(map(decode, encoded))
            xyz = apply(P3, linear)
            wide_chrm = rgb_matrix([('0.67', '0.32'), ('0.265', '0.69'),
                                    ('0.15', '0.06')])
            values['wide_chrm_linear'].append(apply(inverse(SRGB),
                                                     apply(wide_chrm, linear)))
            icc_linear = [(a * v + b) ** gamma if v >= knee else c * v
                          for v in encoded]
            icc_xyz = apply(icc_matrix, icc_linear)
            values['p3_xyz'].append(adapt(xyz))
            values['p3_linear'].append(apply(inverse(SRGB), xyz))
            values['icc_xyz'].append(icc_xyz)
            values['icc_linear'].append(apply(inverse(SRGB),
                                              apply(inverse(adaptation), icc_xyz)))
            values['srgb_linear'].append(linear)
        for key in expected:
            expected[key].append(values[key])
        for channels in (3, 4):
            mode = 'rgb' if channels == 3 else 'rgba'
            packed = [[sample[:channels] for sample in samples[y:y + 4]]
                      for y in (0, 4)]
            raw = b''
            for row in packed:
                raw += b'\0'
                raw += b''.join(bytes(pixel) if depth == 8 else
                                struct.pack('>4H' if channels == 4 else '>3H',
                                            *pixel) for pixel in row)
            ihdr = chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 2, depth,
                         6 if channels == 4 else 2, 0, 0, 0))
            for variant in ('p3', 'raw', 'p3-oriented', 'p3-background', 'wide-chrm'):
                metadata = b''
                if variant not in ('raw', 'wide-chrm'):
                    metadata += chunk(b'iCCP', b'Independent P3\0\0'
                                      + zlib.compress(profile, 9))
                if variant == 'wide-chrm':
                    metadata += chunk(b'cHRM', struct.pack('>8I',
                        31270, 32900, 67000, 32000, 26500, 69000, 15000, 6000))
                if variant == 'p3-oriented':
                    # A single SHORT Orientation tag with inline value 6.
                    tiff = (b'II*\0' + struct.pack('<IH', 8, 1)
                            + struct.pack('<HHIHH', 0x112, 3, 1, 6, 0)
                            + struct.pack('<I', 0))
                    metadata += chunk(b'eXIf', tiff)
                if variant == 'p3-background':
                    metadata += chunk(b'bKGD', struct.pack('>3H', 0, peak, 0))
                name = f'png-{variant}-{depth}-{mode}.png'
                data = (b'\x89PNG\r\n\x1a\n' + ihdr + metadata
                        + chunk(b'IDAT', zlib.compress(raw, 9))
                        + chunk(b'IEND', b''))
                outputs[name] = data
                records.append({'file': name, 'width': 4, 'height': 2,
                    'bit_depth': depth, 'channels': channels,
                    'samples': [sample[:channels] for sample in samples],
                    'orientation': 6 if variant == 'p3-oriented' else 1,
                    'background': [0, peak, 0] if variant == 'p3-background'
                                  else None,
                    'definition': ('cHRM D65/(.67,.32)/(.265,.69)/(.15,.06); '
                                   'default sRGB transfer' if variant == 'wide-chrm'
                                   else 'display-p3.icc' if variant != 'raw' else None),
                    'sha256': hashlib.sha256(data).hexdigest()})
    header += '};\n'
    for key, depths in expected.items():
        header += f'static double const color_png_{key}[2][8][3] = {{\n'
        for rows in depths:
            header += '    {\n'
            for row in rows:
                header += '        {' + fmt(row) + '},\n'
            header += '    },\n'
        header += '};\n'
    outputs['png-samples.h'] = header.encode()
    outputs['png-samples.json'] = (json.dumps({
        'generator': 'tools/generate_color_fixtures.py',
        'arithmetic': '60-digit Decimal; no production transforms',
        'icc_expectations': 'Exact s15Fixed16 matrix and parametric curve fields',
        'alpha': 'Independent coverage: source alpha integer / source peak',
        'pngs': records,
        'expected': {key: [[[float(v) for v in row] for row in rows]
                           for rows in depths] for key, depths in expected.items()}
    }, indent=2) + '\n').encode()
    return outputs



def cmyk_sample_oracles():
    """An affine CMYK input model with a complete v4 Input profile.

    The synthetic D50-referred model is not a characterization of a printer.
    Integer-affine CLUT vertices avoid making the oracle depend on a CMM's
    choice of multidimensional interpolation. XYZ endpoint units are u1.15.
    """
    base = [int((v * 32768).to_integral_value()) for v in D50]
    columns = [adapt(column) for column in zip(*SRGB)]
    weights = [[int((D('.9') * v * 32768).to_integral_value())
                for v in column] for column in columns]
    weights.append([base[j] - sum(w[j] for w in weights) for j in range(3)])
    vertices = [[base[j] - sum(bit * weights[k][j]
                              for k, bit in enumerate(bits))
                 for j in range(3)]
                for bits in itertools.product((0, 1), repeat=4)]
    assert all(0 <= value <= 65535 for row in vertices for value in row)
    lut = b'mft2' + bytes(4) + bytes([4, 3, 2, 0])
    lut += b''.join(fixed(D(i == j)) for i in range(3) for j in range(3))
    lut += struct.pack('>HH', 2, 2)
    lut += struct.pack('>2H', 0, 65535) * 4
    lut += b''.join(struct.pack('>3H', *row) for row in vertices)
    lut += struct.pack('>2H', 0, 65535) * 3
    # Slot 1 makes the physical colorimetric model directly selectable.
    # Slot 0 is required for an Input profile; perceptual CMM adjustments
    # are deliberately outside the colorimetric numeric oracle below.
    profile = pack_profile([(b'A2B0', lut), (b'A2B1', lut)], b'scnr', b'CMYK',
                           'Independent affine CMYK PCS')
    # A separate narrow input-curve ramp makes 32768/65535 observably
    # different from 0.5, even after an external CMM's PCS quantization.
    knee_lut = lut[:48] + struct.pack('>HH', 256, 2)
    knee_lut += struct.pack('>256H', *([0] * 128 + [65535] * 128)) * 4
    knee_lut += b''.join(struct.pack('>3H', *row) for row in vertices)
    knee_lut += struct.pack('>2H', 0, 65535) * 3
    knee_profile = pack_profile([(b'A2B0', knee_lut), (b'A2B1', knee_lut)],
        b'scnr', b'CMYK', 'Independent CMYK input curve knee')
    samples = [[0, 255, 255, 0], [255, 0, 255, 0], [255, 255, 0, 0],
               [0, 0, 0, 0], [255, 255, 255, 255], [64, 128, 192, 32],
               [127, 127, 127, 0], [255, 0, 0, 255]]
    samples16 = [[0, 65535, 65535, 0], [65535, 0, 65535, 0],
                 [65535, 65535, 0, 0], [0, 0, 0, 0], [65535] * 4,
                 [16384, 32768, 49152, 8192], [32768, 32768, 32768, 0],
                 [65535, 0, 0, 65535]]
    inverse_srgb50 = inverse([list(row) for row in zip(*columns)])
    xyz = [[(D(base[j]) - sum(D(v) / 255 * weights[k][j]
                            for k, v in enumerate(sample))) / 32768
            for j in range(3)] for sample in samples]
    linear = [apply(inverse_srgb50, row) for row in xyz]
    gamma = [list(map(encode, row)) for row in linear]
    xyz16 = [[(D(base[j]) - sum(D(v) / 65535 * weights[k][j]
                              for k, v in enumerate(sample))) / 32768
              for j in range(3)] for sample in samples16]
    linear16 = [apply(inverse_srgb50, row) for row in xyz16]
    gamma16 = [list(map(encode, row)) for row in linear16]
    knee_units = [[min(D(1), max(D(0), D(v) / 65535 * 255 - 127))
                   for v in sample] for sample in samples16]
    knee_xyz = [[(D(base[j]) - sum(v * weights[k][j]
                                 for k, v in enumerate(sample))) / 32768
                 for j in range(3)] for sample in knee_units]
    knee_linear = [apply(inverse_srgb50, row) for row in knee_xyz]
    # Propagate a two-bin u1.15 PCS precision budget through XYZ-to-RGB.
    # Backends retaining float PCS use their tighter float-rounding gate.
    lut_bound = [D(2) / 32768 * sum(abs(v) for v in row)
                 for row in inverse_srgb50]
    # PSD stores inverted CMYK, in four raw channel planes.
    resource = (b'8BIM' + struct.pack('>H', 1039) + bytes(2)
                + struct.pack('>I', len(profile)) + profile)
    resource += bytes(len(resource) % 2)
    psd = b'8BPS' + struct.pack('>H', 1) + bytes(6)
    psd += struct.pack('>HIIHH', 4, 1, len(samples), 8, 4)
    psd += bytes(4) + struct.pack('>I', len(resource)) + resource
    psd += bytes(4) + bytes(2)
    psd += bytes(255 - sample[channel] for channel in range(4)
                 for sample in samples)
    raster = bytes(int(min(D(1), max(D(0), v)) * 255 + D('.5'))
                   for row in gamma for v in row)
    reference = (b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', len(samples), 1, 8, 2, 0, 0, 0))
        + chunk(b'sRGB', bytes(1))
        + chunk(b'IDAT', zlib.compress(bytes(1) + raster, 9)) + chunk(b'IEND', b''))
    header = '/* Generated by tools/generate_color_fixtures.py. */\n'
    for symbol, data in [('icc', profile), ('knee_icc', knee_profile)]:
        header += f'static unsigned char const color_cmyk_{symbol}[] = {{\n'
        for offset in range(0, len(data), 12):
            header += '    ' + ', '.join(
                f'0x{v:02x}' for v in data[offset:offset+12]) + ',\n'
        header += '};\n'
    for name, rows in [('source', samples), ('xyz', xyz),
                       ('linear', linear), ('gamma', gamma),
                       ('source16', samples16), ('xyz16', xyz16),
                       ('linear16', linear16), ('gamma16', gamma16),
                       ('knee_linear16', knee_linear)]:
        header += f'static double const color_cmyk_{name}[8][{len(rows[0])}] = {{\n'
        for row in rows:
            header += '    {' + fmt(row) + '},\n'
        header += '};\n'
    header += 'static double const color_cmyk_lut_bound[3] = {\n'
    header += '    ' + fmt(lut_bound) + '\n};\n'
    record = {'generator': 'tools/generate_color_fixtures.py',
        'model': 'Synthetic D50-referred affine CMYK, not a real printer',
        'arithmetic': '60-digit Decimal; no CMM or production decoder',
        'profile_class': 'scnr', 'input': 'CMYK', 'PCS': 'XYZ u1.15',
        'required_tags': ['A2B0', 'desc', 'cprt', 'wtpt'],
        'numeric_intent': 'relative, explicit A2B1; no perceptual oracle',
        'adopted_white': 'D50; no chromaticAdaptationTag required',
        'lut_matrix': 'identity; input is not PCSXYZ',
        'base_u1_15': base, 'weights_u1_15': weights, 'source': samples,
        'xyz': [[float(v) for v in row] for row in xyz],
        'linear': [[float(v) for v in row] for row in linear],
        'gamma': [[float(v) for v in row] for row in gamma],
        'source16': samples16,
        'xyz16': [[float(v) for v in row] for row in xyz16],
        'linear16': [[float(v) for v in row] for row in linear16],
        'gamma16': [[float(v) for v in row] for row in gamma16],
        'input_knee_control': {
            'profile': 'cmyk-knee.icc',
            'profile_sha256': hashlib.sha256(knee_profile).hexdigest(),
            'input_curve': '256 entries: 128 zeros followed by 128 ones',
            'formula': 'clamp(source16 / 65535 * 255 - 127, 0, 1)',
            'purpose': 'Reject normalization by 65536 at original word input',
            'xyz16': [[float(v) for v in row] for row in knee_xyz],
            'linear16': [[float(v) for v in row] for row in knee_linear]},
        'lut_precision_budget': {
            'PCS_bins': 2, 'XYZ_step': 1 / 32768,
            'formula': '2 / 32768 * row_sum(abs(inverse_srgb_D50))',
            'linear_rgb': list(map(float, lut_bound)),
            'builtin': 'float rounding only; this fixed-point budget not used'},
        'reference_rgb8': list(raster),
        'profile_sha256': hashlib.sha256(profile).hexdigest(),
        'psd_sha256': hashlib.sha256(psd).hexdigest()}
    return {'cmyk.icc': profile, 'cmyk-knee.icc': knee_profile,
            'cmyk.psd': psd, 'cmyk-reference.png': reference,
            'cmyk.h': header.encode(),
            'cmyk.json': (json.dumps(record, indent=2) + '\n').encode()}


def jpeg_sample_oracles():
    """Recompute a legacy JPEG reference without a libsixel/CMM golden."""
    path = 'tests/data/inputs/formats/snake-64-embedded-esrgb.jpg'
    data = (ROOT / path).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    assert digest == 'fb0df9b3753bae4a9c3c7b563dfcc2e123c31418b53ff18531213c53b32dfadc'
    # Independently extracted by verify_jpeg_color_source.c, libjpeg-turbo
    # 3.2.0, JCS_YCbCr/JDCT_ISLOW/fancy upsampling. No RGB or ICC conversion.
    samples = [[135, 121, 133], [150, 98, 137], [143, 123, 132]]
    parts = {}
    count = None
    offset = 2
    assert data[:2] == b'\xff\xd8'
    while data[offset:offset+2] != b'\xff\xda':
        assert data[offset] == 255
        marker = data[offset + 1]
        size = struct.unpack_from('>H', data, offset + 2)[0]
        payload = data[offset+4:offset+2+size]
        if marker == 0xe2 and payload.startswith(b'ICC_PROFILE\0'):
            sequence, total = payload[12:14]
            assert 1 <= sequence <= total and sequence not in parts
            assert count is None or count == total
            count = total
            parts[sequence] = payload[14:]
        offset += 2 + size
    assert count == len(parts)
    profile = b''.join(parts[i] for i in range(1, count + 1))
    assert struct.unpack_from('>I', profile)[0] == len(profile)
    assert profile[36:40] == b'acsp' and profile[16:24] == b'RGB XYZ '
    tags = {}
    for offset in range(132, 132 + 12 * struct.unpack_from('>I', profile, 128)[0], 12):
        name, start, size = struct.unpack_from('>4sII', profile, offset)
        tags[name] = profile[start:start+size]
    columns = [[D(v) / 65536 for v in struct.unpack_from('>3i', tags[name], 8)]
               for name in (b'rXYZ', b'gXYZ', b'bXYZ')]
    matrix50 = list(map(list, zip(*columns)))
    curves = []
    for name in (b'rTRC', b'gTRC', b'bTRC'):
        curve = tags[name]
        count = struct.unpack_from('>I', curve, 8)[0]
        assert curve[:4] == b'curv' and count == 2048
        curves.append([D(v) / 65535 for v in struct.unpack_from(
            '>' + str(count) + 'H', curve, 12)])
    inverse_srgb50 = inverse(list(map(list, zip(*[
        adapt(column) for column in zip(*SRGB)]) )))
    raw, xyz, rgb = [], [], []
    for y, cb, cr in samples:
        y, cb, cr = D(y), D(cb - 128), D(cr - 128)
        # Solve JFIF's .299/.587/.114 luma equations, rather than copying
        # the decoder's rounded .34414/.71414 inverse coefficients.
        source = [(y + D('1.402') * cr) / 255,
                  (y - D('.202008') / D('.587') * cb
                   - D('.419198') / D('.587') * cr) / 255,
                  (y + D('1.772') * cb) / 255]
        units = []
        for value, curve in zip(source, curves):
            position = value * (len(curve) - 1)
            index = int(position)
            fraction = position - index
            units.append(curve[index] * (1 - fraction)
                         + curve[index + 1] * fraction)
        physical = apply(matrix50, units)
        raw.append(source)
        xyz.append(physical)
        rgb.append(list(map(encode, apply(inverse_srgb50, physical))))
    record = {'generator': 'tools/generate_color_fixtures.py',
        'input': path, 'sha256': digest,
        'introduced': '51c6dbe9e7ea5a3f98663560ed5cd0a6eb290b08',
        'old_numeric_test': '39c99395fd8c74dc32c5b56e9d248f475d04e6c3',
        'old_literal_generator': 'unresolved; not used by this oracle',
        'source_probe': 'tools/verify_jpeg_color_source.c',
        'source_decoder': 'libjpeg-turbo 3.2.0; YCbCr, integer slow IDCT, fancy upsampling',
        'sample_pixels': [0, 2048, 4095], 'YCbCr': samples,
        'equations': 'https://www.w3.org/Graphics/JPEG/jfif.pdf',
        'arithmetic': '60-digit Decimal; JFIF inverse, stored ICC, derived sRGB D50 inverse',
        'profile_sha256': hashlib.sha256(profile).hexdigest(),
        'profile_description': 'Artifex Software esRGB ICCProfile',
        'matrix_columns_s15Fixed16': [[float(v) for v in row] for row in columns],
        'TRC': 'three curv tables, 2048 u16 entries each; linear interpolation',
        'raw_rgb': [[float(v) for v in row] for row in raw],
        'XYZ_D50': [[float(v) for v in row] for row in xyz],
        'srgb': [[float(v) for v in row] for row in rgb],
        'bounds': {'raw': 5e-7, 'srgb': 1e-6,
            'reason': 'Rounded decoder YCbCr coefficients and float storage; old CMS threshold unchanged'}}
    header = '/* Generated independently from libjpeg YCbCr and stored ICC. */\n'
    for name, rows in [('raw', raw), ('srgb', rgb)]:
        header += f'static float const color_jpeg_{name}[9] = {{\n'
        for row in rows:
            header += '    ' + ', '.join(format(float(v), '.17g') + 'f'
                                       for v in row) + ',\n'
        header += '};\n'
    return {'jpeg.h': header.encode(),
            'jpeg.json': (json.dumps(record, indent=2) + '\n').encode()}


def cli_rgb_oracles():
    """Encode known RGB patches directly, without a codec or CMM golden."""
    size = 64
    samples = list(itertools.product((32, 128), repeat=3))
    pixels = bytes(v for y in range(size) for x in range(size)
                   for v in samples[((y // 16) * 4 + x // 16) % 8])
    columns = [adapt(column) for column in zip(*SRGB)]
    stored = [[D(struct.unpack('>i', fixed(v))[0]) / 65536 for v in col]
              for col in columns]
    tags = [(name, b'XYZ ' + bytes(4) + b''.join(map(fixed, col)))
            for name, col in zip((b'rXYZ', b'gXYZ', b'bXYZ'), columns)]
    curve = b'para' + bytes(4) + struct.pack('>HH', 0, 0) + fixed(D(1))
    tags.extend((name, curve) for name in (b'rTRC', b'gTRC', b'bTRC'))
    adaptation = list(zip(*(adapt([D(i == j) for i in range(3)])
                             for j in range(3))))
    tags.append((b'chad', b'sf32' + bytes(4) + b''.join(
        fixed(v) for row in adaptation for v in row)))
    profile = pack_profile(tags, name='Independent linear sRGB patches')
    srgb_inverse = inverse(list(map(list, zip(*columns))))
    xyz = [apply(list(map(list, zip(*stored))), [D(v) / 255 for v in rgb])
           for rgb in samples]
    gamma = [list(map(encode, apply(srgb_inverse, row))) for row in xyz]
    reference_samples = [bytes(int((v * 255).to_integral_value()) for v in row)
                         for row in gamma]
    reference = b''.join(reference_samples[((y // 16) * 4 + x // 16) % 8]
                         for y in range(size) for x in range(size))

    def png(raster, metadata):
        rows = b''.join(b'\0' + raster[y*size*3:(y+1)*size*3]
                        for y in range(size))
        return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(
            '>IIBBBBB', size, size, 8, 2, 0, 0, 0)) + metadata
            + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))

    outputs = {'cli-rgb.icc': profile,
        'cli-rgb.png': png(pixels, chunk(b'iCCP',
            b'Independent linear RGB\0\0' + zlib.compress(profile, 9))),
        'cli-rgb-reference.png': png(reference, chunk(b'sRGB', b'\0'))}

    # Baseline RGB JPEG: constant 8x8 blocks have only a DC coefficient.
    # DQT=1 makes DC=(sample-128)*8 exact, with no YCbCr or chroma sampling.
    # T.81 Annexes B/C/F define the marker, Huffman and difference syntax.
    def segment(marker, payload):
        return bytes((255, marker)) + struct.pack('>H', len(payload) + 2) + payload

    jpeg_bits = []
    previous = [0, 0, 0]
    for y in range(0, size, 8):
        for x in range(0, size, 8):
            for channel in range(3):
                dc = (pixels[(y * size + x) * 3 + channel] - 128) * 8
                difference = dc - previous[channel]
                previous[channel] = dc
                count = abs(difference).bit_length()
                jpeg_bits.append(format(count, '04b'))
                if count:
                    amplitude = difference if difference > 0 else (
                        difference + (1 << count) - 1)
                    jpeg_bits.append(format(amplitude, f'0{count}b'))
                jpeg_bits.append('0')  # EOB; all 63 AC coefficients are zero.
    bits = ''.join(jpeg_bits)
    bits += '1' * (-len(bits) % 8)
    entropy = bytes(int(bits[i:i+8], 2) for i in range(0, len(bits), 8))
    entropy = entropy.replace(b'\xff', b'\xff\0')
    dc_table = bytes([0]) + bytes([0, 0, 0, 12] + [0] * 12) + bytes(range(12))
    ac_table = bytes([16]) + bytes([1] + [0] * 15) + bytes([0])
    outputs['cli-rgb.jpg'] = (b'\xff\xd8'
        + segment(0xee, b'Adobe' + struct.pack('>HHHB', 100, 0, 0, 0))
        + segment(0xe2, b'ICC_PROFILE\0\1\1' + profile)
        + segment(0xdb, bytes([0]) + bytes([1] * 64))
        + segment(0xc0, struct.pack('>BHHB', 8, size, size, 3)
                  + b'R\x11\0G\x11\0B\x11\0')
        + segment(0xc4, dc_table + ac_table)
        + segment(0xda, b'\3R\0G\0B\0\0\x3f\0')
        + entropy + b'\xff\xd9')

    # Literal-only WebP lossless, with simple two-symbol G/R/B trees.
    # No transforms, caches, backward references or color interpretation.
    webp_bits = []
    def put(value, count):
        webp_bits.extend((value >> i) & 1 for i in range(count))
    put(0, 3)  # No transform, no color cache, one prefix-code group.
    for symbols in [(32, 128)] * 3 + [(255,), (0,)]:
        put(1, 1)
        put(len(symbols) - 1, 1)
        put(symbols[0] > 1, 1)
        put(symbols[0], 8 if symbols[0] > 1 else 1)
        if len(symbols) == 2:
            put(symbols[1], 8)
    for offset in range(0, len(pixels), 3):
        for channel in (1, 0, 2):
            put(pixels[offset + channel] == 128, 1)
    webp_bits.extend([0] * (-len(webp_bits) % 8))
    entropy = bytes(sum(webp_bits[i + j] << j for j in range(8))
                    for i in range(0, len(webp_bits), 8))
    def riff_chunk(name, data):
        return name + struct.pack('<I', len(data)) + data + bytes(len(data) % 2)
    vp8x = bytes([32, 0, 0, 0]) + (size - 1).to_bytes(3, 'little') * 2
    vp8l = b'\x2f' + struct.pack('<I', (size - 1) | ((size - 1) << 14)) + entropy
    body = (b'WEBP' + riff_chunk(b'VP8X', vp8x)
            + riff_chunk(b'ICCP', profile) + riff_chunk(b'VP8L', vp8l))
    outputs['cli-rgb.webp'] = b'RIFF' + struct.pack('<I', len(body)) + body

    # One uncompressed RGB TIFF strip; ICC tag 34675 is the only color hint.
    entries = [(256, 4, 1, struct.pack('<I', size)),
        (257, 4, 1, struct.pack('<I', size)),
        (258, 3, 3, struct.pack('<3H', 8, 8, 8)),
        (259, 3, 1, struct.pack('<H', 1)),
        (262, 3, 1, struct.pack('<H', 2)),
        (273, 4, 1, None), (277, 3, 1, struct.pack('<H', 3)),
        (278, 4, 1, struct.pack('<I', size)),
        (279, 4, 1, struct.pack('<I', len(pixels))),
        (284, 3, 1, struct.pack('<H', 1)),
        (34675, 7, len(profile), profile)]
    offset = 8 + 2 + len(entries) * 12 + 4
    raster_offset = offset + sum((len(v) + 1) & ~1 for _, _, _, v in entries
                                 if v is not None and len(v) > 4)
    table, payload = b'', b''
    for tag, kind, count, value in entries:
        if value is None:
            value = struct.pack('<I', raster_offset)
        if len(value) <= 4:
            field = value + bytes(4 - len(value))
        else:
            field = struct.pack('<I', offset)
            value += bytes(len(value) % 2)
            payload += value
            offset += len(value)
        table += struct.pack('<HHI', tag, kind, count) + field
    outputs['cli-rgb.tiff'] = (b'II\x2a\0' + struct.pack('<I', 8)
        + struct.pack('<H', len(entries)) + table + bytes(4) + payload + pixels)
    record = {'generator': 'tools/generate_color_fixtures.py',
        'arithmetic': '60-digit Decimal, original integer RGB / 255, stored ICC matrix, independent XYZ D50 to sRGB',
        'size': [size, size], 'patch_size': [16, 16],
        'patch_index': '((y // 16) * 4 + x // 16) % 8',
        'source_RGB8': samples, 'XYZ_D50': [[float(v) for v in row] for row in xyz],
        'gamma_sRGB': [[float(v) for v in row] for row in gamma],
        'reference_RGB8': [list(row) for row in reference_samples],
        'profile': 'complete v4 RGB matrix/TRC, linear transfer, D65 adopted white, Bradford chad to ICC D50',
        'JPEG': 'SOF0, direct RGB, unit quantization, constant 8x8 DC blocks, no YCbCr or chroma subsampling',
        'WebP': 'VP8L, literal-only simple two-symbol RGB trees, no transforms',
        'TIFF': 'one uncompressed contiguous RGB strip, ICC tag 34675',
        'sources': ['https://www.w3.org/Graphics/JPEG/itu-t81.pdf',
            'https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification',
            'https://www.color.org/specification/ICC.1-2022-05.pdf'],
        'sha256': {name: hashlib.sha256(data).hexdigest()
                   for name, data in outputs.items()}}
    outputs['cli-rgb.json'] = (json.dumps(record, indent=2) + '\n').encode()
    return outputs


def hdr_sample_oracles():
    """Decode the legacy one-pixel RGBE specimens without a loader or CMM."""
    files = ['stbi_midtones.hdr', 'stbi_midtones_hdrmeta_gamma22.hdr',
             'stbi_midtones_hdrmeta_bt2020.hdr',
             'stbi_midtones_hdrmeta_gamma22_bt2020.hdr']
    specimens = []
    values = []
    for name in files:
        data = (ROOT / 'tests/data/inputs/formats' / name).read_bytes()
        header, payload = data.split(b'\n-Y 1 +X 1\n')
        assert len(payload) == 4 and payload == bytes([128, 64, 32, 128])
        source = [D(v) * D(2) ** (payload[3] - 136) for v in payload[:3]]
        gamma = D(1)
        primaries = [('0.64', '0.33'), ('0.30', '0.60'), ('0.15', '0.06')]
        for line in header.decode('ascii').splitlines():
            if line.startswith('GAMMA='):
                gamma = D(line.split('=')[1])
            if line.startswith('PRIMARIES='):
                xy = line.split('=')[1].split()
                assert [D(v) for v in xy[6:]] == [D('.3127'), D('.3290')]
                primaries = list(zip(xy[:6:2], xy[1:6:2]))
        decoded = [v ** (1 / gamma) for v in source]
        xyz = apply(rgb_matrix(primaries), decoded)
        linear = apply(inverse(SRGB), xyz)
        values.append(linear)
        specimens.append({'file': name, 'sha256': hashlib.sha256(data).hexdigest(),
            'header': header.decode('ascii'), 'RGBE': list(payload),
            'source': list(map(float, source)), 'source_gamma': float(gamma),
            'primaries': primaries, 'white_xy': [.3127, .3290],
            'XYZ_D65': list(map(float, xyz)),
            'linear_sRGB': list(map(float, linear)),
            'EV2_gamma_no_tonemap': [float(encode(v * 4)) for v in linear],
            'EV2_gamma_reinhard': [float(encode(v * 4 / (1 + v * 4)))
                                   for v in linear]})
    # Without header hints, explicit sRGB fallback decodes the original RGBE
    # triplet with the sRGB curve. Header gamma/primaries take precedence.
    values.append(list(map(decode, source)))
    header = '/* Generated by tools/generate_color_fixtures.py. */\n'
    header += 'static double const color_hdr_linear[5][3] = {\n'
    header += ''.join('    {' + fmt(row) + '},\n' for row in values) + '};\n'
    record = {'generator': 'tools/generate_color_fixtures.py',
        'arithmetic': '60-digit Decimal; independent RGBE, matrix and TRC equations',
        'RGBE_equation': 'mantissa * 2 ** (exponent - 136)',
        'RGBE_convention': 'Existing libsixel lower-edge reconstruction; Radiance colr_color uses a +0.5 mantissa midpoint',
        'header_gamma_policy': 'source component ** (1 / GAMMA)',
        'matrix_policy': 'original header xy to XYZ D65 to linear sRGB; shared D65',
        'fixture_caveat': 'The nominal BT.2020 fixture stores red y=0.2919, not 0.2920; use stored xy.',
        'specimens': specimens,
        'srgb_fallback_linear': list(map(float, values[-1])),
        'source': 'https://radsite.lbl.gov/radiance/refer/Notes/picture_format.html'}
    return {'hdr.h': header.encode(),
            'hdr.json': (json.dumps(record, indent=2) + '\n').encode()}


def palette_export_oracles():
    """An exact storage-conversion oracle, independent of solver centroids."""
    rows = []
    for encoded in [[0, 128, 128], [255, 128, 128]]:
        lightness = D(encoded[0]) / 255 * 100
        a = (D(encoded[1]) / 255 - D('.5')) * 3 * 128
        b = (D(encoded[2]) / 255 - D('.5')) * 3 * 128
        fy = (lightness + 16) / 116
        f = [fy + a / 500, fy, fy - b / 200]
        xyz = [w * (v ** 3 if v > D(6) / 29 else
                    (v - D(4) / 29) * D(108) / 841)
               for w, v in zip(map(D, ['.95047', '1', '1.08883']), f)]
        linear = apply(matrix([
            ['3.2404542', '-1.5371385', '-.4985314'],
            ['-.9692660', '1.8760108', '.0415560'],
            ['.0556434', '-.2040259', '1.0572252']]), xyz)
        gamma = list(map(encode, linear))
        rgb8 = [int(min(D(1), max(D(0), v)) * 255 + D('.5')) for v in gamma]
        rows.append({'CIELAB8': encoded, 'CIELAB': list(map(float, [lightness, a, b])),
            'XYZ_D65': list(map(float, xyz)), 'linear_sRGB': list(map(float, linear)),
            'gamma_sRGB': list(map(float, gamma)), 'RGB8': rgb8})
    record = {'generator': 'tools/generate_color_fixtures.py',
        'arithmetic': '60-digit Decimal; CIELAB inverse, D65 RGB matrix and sRGB TRC',
        'source': 'PPM black and white; two palette entries with binning disabled',
        'storage': 'Existing byte palette: L/100 * 255; (a/128/3 + 0.5) * 255, rounded',
        'boundary': 'CIELAB8 to float coordinates to bounded gamma RGB8 export',
        'limitation': 'CIELAB8 has no exact neutral a/b code; this tests existing export storage, not canonical float palette accuracy',
        'rows': rows}
    return {'palette-export.json': (json.dumps(record, indent=2) + '\n').encode()}


def legacy_png_icc_oracle():
    """Recompute the existing PNG probe without a decoder or CMS oracle."""
    path = ROOT / 'tests/data/inputs/formats/map8_embedded_icc.png'
    data = path.read_bytes()
    chunks, offset = {}, 8
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        tag = data[offset + 4:offset + 8]
        chunks.setdefault(tag, []).append(data[offset + 8:offset + 8 + size])
        offset += size + 12
    width, height, depth, model = struct.unpack_from('>IIBB', chunks[b'IHDR'][0])
    assert (width, height, depth, model) == (93, 14, 8, 6)
    scanlines = zlib.decompress(b''.join(chunks[b'IDAT']))
    raster, previous = [], [0] * (width * 4)
    for y in range(height):
        start = y * (width * 4 + 1)
        mode = scanlines[start]
        row = list(scanlines[start + 1:start + 1 + width * 4])
        for i, value in enumerate(row):
            left = row[i - 4] if i >= 4 else 0
            up = previous[i]
            corner = previous[i - 4] if i >= 4 else 0
            prediction = left + up - corner
            distances = [abs(prediction - v) for v in (left, up, corner)]
            paeth = (left, up, corner)[distances.index(min(distances))]
            predictor = (0, left, up, (left + up) // 2, paeth)[mode]
            row[i] = (value + predictor) % 256
        raster.extend(row)
        previous = row
    compressed = chunks[b'iCCP'][0]
    profile = zlib.decompress(compressed[compressed.index(0) + 2:])
    tags = {}
    for i in range(struct.unpack_from('>I', profile, 128)[0]):
        tag, offset, size = struct.unpack_from('>4sII', profile, 132 + i * 12)
        tags[tag] = profile[offset:offset + size]
    columns = [[D(v) / 65536 for v in struct.unpack_from(
        '>iii', tags[channel + b'XYZ'], 8)] for channel in (b'r', b'g', b'b')]
    source = [list(row) for row in zip(*columns)]
    destination = [list(row) for row in zip(*(
        adapt(column) for column in zip(*SRGB)))]
    records = []
    for pixel in (0, 651, 1301):
        rgba = raster[pixel * 4:pixel * 4 + 4]
        linear = []
        for sample, channel in zip(rgba, (b'r', b'g', b'b')):
            tag = tags[channel + b'TRC']
            assert tag[:4] == b'curv'
            count = struct.unpack_from('>I', tag, 8)[0]
            assert count == 1024
            curve = struct.unpack_from('>' + str(count) + 'H', tag, 12)
            position = D(sample) / 255 * (count - 1)
            low = int(position)
            high = min(low + 1, count - 1)
            linear.append((curve[low] + (curve[high] - curve[low]) *
                           (position - low)) / 65535)
        output = apply(inverse(destination), apply(source, linear))
        records.append({'pixel': pixel, 'rgba': rgba,
                        'linear_srgb': [float(v) for v in output]})
    manifest = {
        'fixture': str(path.relative_to(ROOT)),
        'sha256': hashlib.sha256(data).hexdigest(),
        'icc_sha256': hashlib.sha256(profile).hexdigest(),
        'method': 'Original RGBA8; stored 1024-point curv interpolation and '
                  'XYZ matrix; independently derived D50 sRGB inverse',
        'samples': records}
    header = '/* Generated by tools/generate_color_fixtures.py. */\n'
    header += 'static float const color_legacy_png_linear[9] = {\n'
    for record in records:
        header += '    ' + ', '.join(format(v, '.12g') + 'f' if v else '0.0f'
                                     for v in record['linear_srgb']) + ',\n'
    header += '};\n'
    return {'legacy-png-icc.h': header.encode(),
            'legacy-png-icc.json': (json.dumps(manifest, indent=2) + '\n').encode()}


def png_byte_target_oracles():
    """Resolve final byte ties from original PNG samples and float storage."""
    outputs = {}
    records = []
    header = '/* Independent original PNG -> stored float -> final byte. */\n'
    header += 'static struct {\n    unsigned int source;\n'
    header += '    unsigned int linear;\n    unsigned int gamma;\n'
    header += '    unsigned char byte;\n} const color_png_byte_target[2] = {\n'
    for index, (gama, sample) in enumerate(((45455, 33524), (33333, 57717))):
        def stored(value):
            packed = struct.pack('>f', float(value))
            return (D.from_float(struct.unpack('>f', packed)[0]),
                    struct.unpack('>I', packed)[0])
        source, source_bits = stored(D(sample) / 65535)
        linear, linear_bits = stored(source ** (D(100000) / gama))
        gamma = encode(linear)
        gamma_float, gamma_bits = stored(gamma)
        final_byte = int(gamma * 255 + D('.5'))
        intermediate_byte = int(gamma_float * 255 + D('.5'))
        assert final_byte != intermediate_byte
        name = f'png-byte-target-{index}.png'
        image = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(
            '>IIBBBBB', 1, 1, 16, 2, 0, 0, 0)) + chunk(b'gAMA',
            struct.pack('>I', gama)) + chunk(b'IDAT', zlib.compress(
            b'\0' + struct.pack('>3H', sample, sample, sample), 9))
            + chunk(b'IEND', b''))
        outputs[name] = image
        records.append({'file': name, 'gAMA': gama, 'source_u16': sample,
            'sha256': hashlib.sha256(image).hexdigest(),
            'source_float': str(source), 'source_bits': source_bits,
            'linear_float': str(linear), 'linear_bits': linear_bits,
            'gamma': str(gamma), 'gamma_float': str(gamma_float),
            'gamma_bits': gamma_bits, 'byte': final_byte,
            'gamma_float_byte': intermediate_byte})
        header += (f'    {{0x{source_bits:08x}u, 0x{linear_bits:08x}u, '
                   f'0x{gamma_bits:08x}u, {final_byte}}},\n')
    header += '};\n'
    outputs['png-byte-target.h'] = header.encode()
    outputs['png-byte-target.json'] = (json.dumps({'arithmetic':
        '60-digit Decimal; explicit IEEE binary32 source/linear storage; '
        'signed sRGB transfer and half-up final byte rounding',
        'cases': records}, indent=2) + '\n').encode()
    return outputs


def frame_byte_oracles():
    """Round exact stored floats only after independent inverse coordinates."""
    def as_bits(value):
        return struct.unpack('>I', struct.pack('>f', float(value)))[0]

    def from_bits(value):
        return D.from_float(struct.unpack('>f', struct.pack('>I', value))[0])

    # Solve the published forward matrices independently; do not call the
    # production inverse or derive expected colors by a production roundtrip.
    lms_inverse = inverse(matrix([
        ['.4122214708', '.5363325363', '.0514459929'],
        ['.2119034982', '.6806995451', '.1073969566'],
        ['.0883024619', '.2817188376', '.6299787005']]))
    oklab_inverse = inverse(matrix([
        ['.2104542553', '.7936177850', '-.0040720468'],
        ['1.9779984951', '-2.4285922050', '.4505937099'],
        ['.0259040371', '.7827717662', '-.8086757660']]))
    xyz_inverse = inverse(matrix([
        ['.4124564', '.3575761', '.1804375'],
        ['.2126729', '.7151522', '.0721750'],
        ['.0193339', '.1191920', '.9503041']]))
    angle = D('3.141592653589793238462643383279502884197169399375105820974944') * 50 / 180
    sine = term = angle
    cosine = cterm = D(1)
    for n in range(1, 60):
        term *= -angle * angle / ((2 * n) * (2 * n + 1))
        cterm *= -angle * angle / ((2 * n - 1) * (2 * n))
        sine += term
        cosine += cterm

    def lab_inverse(value, din=False):
        light, a, b = value
        if din:
            light, a, b = light * 100, a * 100, b * 100
            light = ((light / D('325.22')).exp() - 1) / D('.0036')
            chroma = (a * a + b * b).sqrt()
            gain = (((chroma / D('22.5')).exp() - 1) /
                    D('.06') / chroma) if chroma else D(0)
            e = gain * (cosine * a + sine * b)
            f = gain * (-sine * a + cosine * b) / D('1.14')
            a, b = cosine * e - sine * f, sine * e + cosine * f
        else:
            light, a, b = light * 100, a * 128, b * 128
        fy = (light + 16) / 116
        roots = [fy + a / 500, fy, fy - b / 200]
        values = [v ** 3 if v > D(6) / 29 else
                  (116 * v - 16) * 27 / 24389 for v in roots]
        white_x = (D('1.12') * D('.95047') - D('.12') * D('1.08883')) if din else D('.95047')
        xyz = [values[0] * white_x, values[1], values[2] * D('1.08883')]
        if din:
            xyz[0] = (xyz[0] + D('.12') * xyz[2]) / D('1.12')
        return apply(xyz_inverse, xyz)

    records, offsets = [], [0]
    names = ['gamma', 'linear', 'oklab', 'cielab', 'din99d']
    for space in names:
        sources = []
        if space in ('gamma', 'linear'):
            values = [as_bits(v) for v in ['-2', '-.1', '-0', '0', '1e-40',
                '.0000001', '.0001', '.001', '.0031307', '.0031308',
                '.0031309', '.01', '.1', '.5', '.9999', '1', '1.2', '2']]
            values += [0x7f7fffff, 0xff7fffff]
            for n in range(255):
                value = (D(n) + D('.5')) / 255
                midpoint = as_bits(decode(value) if space == 'linear' else value)
                values += [midpoint - 1, midpoint, midpoint + 1]
            sources = [[bits, values[(i + 1) % len(values)],
                        values[(i + 2) % len(values)]]
                       for i, bits in enumerate(values)]
        else:
            samples = [[D('.5'), D(0), D(0)], [D('.65'), D('.2'), D('-.1')],
                       [D('1.2'), D('-.7'), D('.4')], [D('-.1'), D(0), D(0)],
                       [D(0), D(0), D(0)], [D(1), D(0), D(0)]]
            samples += [list(map(D, row)) for row in
                        [('0.2', '-0.02', '0.03'), ('0.01', '0', '0'),
                         ('0.75', '0.1', '-0.15')]]
            sources = [[as_bits(v) for v in row] for row in samples]
            # These fit double throughout but cannot pass through an
            # intermediate linear float32 buffer. The byte result is valid.
            sources += ([[0x7f7fffff, 0, 0], [0xff7fffff, 0, 0]]
                        if space != 'din99d' else [[as_bits(100), 0, 0]])
        for bits in sources:
            source = [from_bits(v) for v in bits]
            if space == 'gamma':
                gamma = source
                linear = None
            else:
                linear = source if space == 'linear' else (
                    apply(lms_inverse, [v ** 3 for v in
                                        apply(oklab_inverse, source)])
                    if space == 'oklab' else
                    lab_inverse(source, space == 'din99d'))
                gamma = list(map(encode, linear))
            encoded = [max(D(0), min(D(1), v)) * 255 for v in gamma]
            byte = [int((v + D('.5')).to_integral_value(rounding='ROUND_FLOOR'))
                    for v in encoded]
            records.append({'space': space, 'source_bits': bits,
                'source_float': [str(v) for v in source],
                'linear': None if linear is None else [str(v) for v in linear],
                'gamma': [str(v) for v in gamma], 'byte': byte,
                'half_byte_distance': [str(abs(v - (int(v) + D('.5'))))
                                       for v in encoded]})
        offsets.append(len(records))
    header = '/* Generated from exact stored floats and independent math. */\n'
    header += 'static unsigned int const color_frame_byte_offsets[6] = {\n'
    header += '    ' + ', '.join(map(str, offsets)) + '\n};\n'
    header += 'static struct {\n    unsigned int bits[3];\n'
    header += '    unsigned char bytes[3];\n} const color_frame_byte[] = {\n'
    for record in records:
        header += '    {{' + ', '.join(f'0x{v:08x}u' for v in record['source_bits'])
        header += '}, {' + ', '.join(map(str, record['byte'])) + '}},\n'
    header += '};\n'
    manifest = {'generator': 'tools/generate_color_fixtures.py',
        'arithmetic': '60-digit Decimal; exact IEEE binary32 source; inverse '
                      'forward Oklab/legacy Lab matrices; scalar DIN99d inverse; '
                      'gamma identity; final clamp and half-up byte rounding',
        'offsets': offsets, 'samples': records}
    return {'frame-byte.h': header.encode(),
            'frame-byte.json': (json.dumps(manifest, indent=2) + '\n').encode()}


def png_palette_oracles(power_records):
    """Build indexed PNGs and expectations from original PLTE, tRNS and ICC."""
    colors = bytes([41, 83, 127, 67, 109, 151, 89, 137, 181, 113, 157, 211])
    indices = bytes([0, 1, 2, 3, 3, 2, 1, 0])
    alpha = [bytes([0, 85, 170]), bytes([85, 0, 0]),
             bytes([1, 85, 170]), bytes(4)]
    names = ['single', 'multi', 'none', 'all']
    record = next(p for p in json.loads(power_records)['profiles']
                  if p['name'] == 'mask0-para0')
    profile = bytes.fromhex(record['profile_hex'])
    stored_matrix = [[D(v) for v in row] for row in record['matrix_stored']]
    gammas = list(map(D, record['gamma_stored']))
    destination = list(map(list, zip(*(adapt(col) for col in zip(*SRGB)))))
    inv_destination = inverse(destination)
    expected = []
    units = []
    for i in range(4):
        linear = apply(inv_destination, apply(stored_matrix,
            [(D(colors[i * 3 + c]) / 255) ** gammas[c] for c in range(3)]))
        unit = [max(D(0), min(D(1), encode(v))) for v in linear]
        expected.extend(int((v * 255 + D('.5')).to_integral_value(
            rounding='ROUND_FLOOR')) for v in unit)
        units.append(list(map(str, unit)))

    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))

    def make_png(transparency, managed):
        raw = b'\0' + indices[:4] + b'\0' + indices[4:]
        return (b'\x89PNG\r\n\x1a\n'
                + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 2, 8, 3, 0, 0, 0))
                + (chunk(b'iCCP', b'independent\0\0' + zlib.compress(profile))
                   if managed else b'')
                + chunk(b'PLTE', colors) + chunk(b'tRNS', transparency)
                + chunk(b'IDAT', zlib.compress(raw, 0)) + chunk(b'IEND', b''))

    header = ('/* Generated from source PLTE/tRNS and stored ICC tags. */\n'
              '#include <stddef.h>\n')

    def array(name, data):
        return (f'static unsigned char const {name}[] = {{\n'
                + ''.join('    ' + ', '.join(f'0x{v:02x}' for v in data[p:p+12])
                          + ',\n' for p in range(0, len(data), 12)) + '};\n')

    header += array('color_pal_rgb', colors)
    header += array('color_pal_indices', indices)
    header += array('color_pal_cms', bytes(expected))
    header += ('static unsigned char const color_pal_alpha[][4] = {\n'
               + ''.join('    { ' + ', '.join(map(str, a.ljust(4, b'\xff')))
                         + ' },\n' for a in alpha) + '};\n')
    outputs = {}
    for name, a in zip(names + ['cms'], alpha + alpha[:1]):
        data = make_png(a, name == 'cms')
        outputs['palette-trns-' + name + '.png'] = data
        header += array('color_pal_png_' + name, data)
    header += ('static struct {\n    unsigned char const *bytes;\n'
               '    size_t length;\n} const color_pal_png[] = {\n'
               + ''.join(f'    {{ color_pal_png_{n}, '
                         f'sizeof(color_pal_png_{n}) }},\n'
                         for n in names + ['cms']) + '};\n')
    outputs['palette-trns.h'] = header.encode()
    outputs['palette-trns.json'] = (json.dumps({
        'generator': 'tools/generate_color_fixtures.py',
        'rgb': colors.hex(), 'indices': indices.hex(),
        'alpha': {n: a.hex() for n, a in zip(names, alpha)},
        'implicit_alpha': 255, 'profile': record,
        'cms_units': units, 'cms_expected': bytes(expected).hex(),
        'cms_model': '60-digit Decimal powers of original RGB8/255, stored XYZ '
                     'matrix, independent inverse sRGB D50, sRGB transfer, '
                     'clamp and nearest byte; no CMM oracle',
        'files_scope': 'PNG fixture bytes; the C header is verified by --check.',
        'files': {n: hashlib.sha256(v).hexdigest() for n, v in outputs.items()
                  if n.endswith('.png')}
    }, indent=2) + '\n').encode()
    return outputs


def png_metadata_oracles():
    """Derive PNG metadata fixtures without importing a product decoder."""
    from decimal import Decimal as N, localcontext

    def mm(a, b):
        return [[sum((a[i][k] * b[k][j] for k in range(len(b)))) for j in range(len(b[0]))] for i in range(len(a))]

    def inv(m):
        # Pivoted elimination is independent of the product's cofactor inverse.
        a = [r[:] + [N(i == j) for j in range(3)] for i, r in enumerate(m)]
        for c in range(3):
            k = max(range(c, 3), key=lambda i: abs(a[i][c]))
            a[c], a[k] = (a[k], a[c])
            div = a[c][c]
            if div == 0:
                raise ValueError('singular')
            a[c] = [v / div for v in a[c]]
            for i in range(3):
                if i != c:
                    q = a[i][c]
                    a[i] = [a[i][j] - q * a[c][j] for j in range(6)]
        return [r[3:] for r in a]

    def space(grid):
        if any((x > 100000 or y <= 0 or x + y > 100000 for x, y in zip(grid[::2], grid[1::2]))):
            raise ValueError('invalid grid')
        xyz = [[N(x) / y, N(1), N(100000 - x - y) / y] for x, y in zip(grid[::2], grid[1::2])]
        m = [[xyz[j + 1][i] for j in range(3)] for i in range(3)]
        scale = mm(inv(m), [[x] for x in xyz[0]])
        if any((v[0] <= 0 for v in scale)):
            raise ValueError('nonpositive primary scale')
        return ([[m[i][j] * scale[j][0] for j in range(3)] for i in range(3)], [[v] for v in xyz[0]])

    def transform(grid):
        src, sw = space(grid)
        dst, dw = space([31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000])
        br = [list(map(N, line.split())) for line in ['.8951 .2664 -.1614', '-.7502 1.7135 .0367', '.0389 -.0685 1.0296']]
        sc, dc = (mm(br, sw), mm(br, dw))
        if any((v[0] <= 0 for v in sc)):
            raise ValueError('nonpositive source cone')
        diag = [[dc[i][0] / sc[i][0] if i == j else N(0) for j in range(3)] for i in range(3)]
        return mm(inv(dst), mm(inv(br), mm(diag, mm(br, src))))

    def f32(x):
        return struct.unpack('>f', struct.pack('>f', float(x)))[0]

    def pack(t, v):
        return struct.pack('>I', len(v)) + t + v + struct.pack('>I', zlib.crc32(t + v))

    def png(depth, kind, pixels, grid, gama=25000, bg=None, srgb=False, apng=False):
        channels = {2: 3, 3: 1, 6: 4}[kind]
        raw = b'\x00' + (bytes(range(len(pixels))) if kind == 3 else b''.join((struct.pack('>H', v) if depth == 16 else bytes([v]) for p in pixels for v in p[:channels])))
        out = b'\x89PNG\r\n\x1a\n' + pack(b'IHDR', struct.pack('>IIBBBBB', len(pixels), 1, depth, kind, 0, 0, 0))
        if grid is not None:
            out += pack(b'cHRM', struct.pack('>8I', *grid))
        if gama is not None:
            out += pack(b'gAMA', struct.pack('>I', gama))
        if srgb:
            out += pack(b'sRGB', b'\x00')
        if kind == 3:
            out += pack(b'PLTE', bytes((v for p in pixels for v in p[:3]))) + pack(b'tRNS', bytes((p[3] for p in pixels)))
        if bg is not None:
            out += pack(b'bKGD', bytes([bg]) if kind == 3 else struct.pack('>3H', *bg))
        if apng:
            out += pack(b'acTL', struct.pack('>2I', 1, 0)) + pack(b'fcTL', struct.pack('>5I2H2B', 0, len(pixels), 1, 0, 0, 1, 10, 0, 0))
        return out + pack(b'IDAT', zlib.compress(raw)) + pack(b'IEND', b'')
    base = [34570, 35850, 70800, 29200, 17000, 79700, 13100, 4600]
    gridzero = base[:]
    gridzero[4:6] = [272, 99728]
    gridinside = gridzero[:]
    gridinside[5] -= 1
    gridoutside = gridzero[:]
    gridoutside[5] += 1
    singular = base[:2] + base[2:4] * 3
    pixels8 = [(0, 0, 0, 0), (255, 255, 255, 255), (10, 20, 30, 40), (50, 100, 150, 85), (200, 80, 30, 192), (255, 0, 0, 255), (0, 255, 0, 0), (0, 0, 255, 255)]
    pixels16 = [
        (0, 0, 0, 0), (65535, 65535, 65535, 65535),
        (2571, 5141, 7711, 10281), (12851, 25701, 38551, 21846),
        (51401, 20561, 7711, 49345), (65535, 0, 0, 65535),
        (0, 65535, 0, 0), (0, 0, 65535, 65535)]
    specs = []
    for depth, pixels in [(8, pixels8), (16, pixels16)]:
        for kind in [2, 6]:
            for label, grid in [
                    ('boundary', base), ('singular', singular),
                    ('grid-zero', gridzero), ('grid-inside', gridinside),
                    ('grid-outside', gridoutside), ('gamma-only', None),
                    ('chrm-only', base), ('srgb', base)]:
                specs.append(dict(name=f'{label}-{depth}-{kind}', depth=depth, kind=kind, pixels=pixels, grid=grid, gama=None if label == 'chrm-only' else 25000, srgb=label == 'srgb'))
        for label, grid in [('boundary', base), ('singular', singular)]:
            specs.append(dict(name=f'{label}-filebg-{depth}', depth=depth, kind=6, pixels=pixels, grid=grid, bg=pixels[4][:3]))
    for label, grid in [('boundary', base), ('singular', singular)]:
        specs.append(dict(name=f'{label}-filebg-indexed', depth=8, kind=3, pixels=pixels8, grid=grid, bg=4))
    specs.append(dict(name='boundary-palette', depth=8, kind=3, pixels=pixels8, grid=base))
    specs.append(dict(name='boundary-apng', depth=8, kind=6, pixels=pixels8, grid=base, apng=True))
    records = []
    outputs = {}
    for spec in specs:
        name = spec['name']
        data = png(**{k: v for k, v in spec.items() if k != 'name'})
        filename = 'png-metadata-' + name + '.png'
        outputs[filename] = data
        records.append(spec | {'file': filename, 'sha256': hashlib.sha256(data).hexdigest()})

    def oracle(s, precision):
        with localcontext() as ctx:
            ctx.prec = precision
            m = None
            reason = 'absent'
            if s['grid'] is not None:
                try:
                    m = transform(s['grid'])
                    reason = 'valid'
                except ValueError as e:
                    reason = str(e)
            maximum = N(65535 if s['depth'] == 16 else 255)
            rows = []
            for p in s['pixels']:
                n = [N(f32(N(v) / maximum)) for v in p[:3]]
                # gAMA=25000 makes the transfer an exact fourth power. Keep
                # this matrix model separate from metadata priority below.
                gamma = [v ** 4 for v in n]
                v = mm(m, [[q] for q in gamma]) if m else [[q] for q in gamma]
                rows.append({
                    'normalized_f32': list(map(str, n)),
                    'alpha': str(N(p[3]) / maximum),
                    'gamma_double': list(map(str, gamma)),
                    'linear_exact': [str(q[0]) for q in v],
                    'linear_f32': [f32(q[0]) for q in v],
                    'intermediate_f32_model': [f32(q[0]) for q in
                        mm(m, [[N(f32(q))] for q in gamma])] if m else None})
            return {'metadata': reason, 'matrix': [[str(q) for q in row] for row in m] if m else None, 'samples': rows}
    header = (
        '/* Generated from original PNG chunks and Decimal80 equations. */\n'
        '#ifndef COLOR_PNG_METADATA_H\n#define COLOR_PNG_METADATA_H\n'
        '#include <stddef.h>\n'
        'typedef struct {\n'
        '    unsigned char const *bytes;\n'
        '    size_t length;\n'
        '    int depth, type, background, usable_profile, transfer;\n'
        '    float linear[24], gamma[24], source[24], srgb[24];\n'
        '    float explicit_gamma[24], explicit_linear[24];\n'
        '    unsigned short rgba[32];\n'
        '    unsigned char byte_rgb[24];\n'
        '} color_png_metadata_t;\n')
    for number, r in enumerate(records):
        data = outputs[r['file']]
        header += f'static unsigned char const color_pm_{number}[] = {{\n'
        header += ''.join(('    ' + ', '.join((f'0x{v:02x}' for v in data[i:i + 12])) + ',\n' for i in range(0, len(data), 12))) + '};\n'
    header += 'static color_png_metadata_t const color_png_metadata[] = {\n'
    for number, r in enumerate(records):
        o = oracle(r, 80)
        r['oracle'] = o
        with localcontext() as ctx:
            ctx.prec = 80
            samples = o['samples']
            valid = o['metadata'] == 'valid'
            units = [[N(v) for v in p['normalized_f32']] for p in samples]
            srgb = [[f32(v / N('12.92') if v <= N('.04045') else ((v + N('.055')) / N('1.055')) ** N('2.4')) for v in row] for row in units]
            gamma = [[f32(N(v)) for v in p['gamma_double']] for p in samples]
            linear = [p['linear_f32'][:] for p in samples]
            transfer = 0 if r.get('srgb') or r.get('gama', 25000) is None else 1
            if not transfer:
                linear = [row[:] for row in srgb]

            def composite(values, bg):
                return [[f32(N(v) * N(p['alpha']) + N(bg[c]) * (1 - N(p['alpha']))) if N(p['alpha']) else v for c, v in enumerate(row)] for row, p in zip(values, samples)]
            exp_unit = [N(f32(N(v) / 255)) for v in [200, 80, 30]]
            exp_gamma = [f32(v / N('12.92') if v <= N('.04045') else ((v + N('.055')) / N('1.055')) ** N('2.4')) for v in exp_unit]
            explicit_gamma = composite(linear, exp_gamma)
            explicit_linear = composite(linear, exp_unit)
            if 'bg' in r:
                linear = composite(linear, linear[4])
                gamma = composite(gamma, gamma[4])
            byte_rgb = []
            # Only the paired gAMA/cHRM palette and APNG tests consume this
            # legacy byte projection; canonical tests use the arrays above.
            m = [[N(v) for v in row] for row in o['matrix']] if valid else None
            for p in r['pixels']:
                v = [(N(c) / (65535 if r['depth'] == 16 else 255)) ** 4 for c in p[:3]]
                if m:
                    v = [q[0] for q in mm(m, [[q] for q in v])]
                for c in v:
                    c = max(N(0), min(N(1), c))
                    e = N('12.92') * c if c <= N('.0031308') else N('1.055') * c ** (N(1) / N('2.4')) - N('.055')
                    byte_rgb.append(int(e * 255 + N('.5')))
            rgba = [v for p in r['pixels'] for v in p]
            arrays = [linear, gamma, units, srgb, explicit_gamma, explicit_linear]
            header += f'    {{ color_pm_{number}, sizeof(color_pm_{number}),\n'
            header += f"      {r['depth']}, {r['kind']}, {int('bg' in r)}, {int(valid or o['metadata'] == 'absent')}, {transfer},\n"
            for values in arrays:
                flat = [v for row in values for v in row]
                header += '      {\n' + ''.join(('        ' + ', '.join((float(v).hex() + 'f' for v in flat[i:i + 3])) + ',\n' for i in range(0, 24, 3))) + '      },\n'
            header += '      {\n' + ''.join(('        ' + ', '.join(map(str, rgba[i:i + 8])) + ',\n' for i in range(0, 32, 8))) + '      },\n'
            header += '      {\n' + ''.join(('        ' + ', '.join(map(str, byte_rgb[i:i + 8])) + ',\n' for i in range(0, 24, 8))) + '      } },\n'
            r['expected'] = {
                name: [[float(v) for v in row] for row in values]
                for name, values in zip([
                    'linear', 'gamma_fallback', 'normalized_source',
                    'default_srgb', 'explicit_gamma', 'explicit_linear'], arrays)}
            r['expected']['byte_rgb'] = byte_rgb
    header += '};\n#endif /* COLOR_PNG_METADATA_H */\n'
    outputs['png-metadata.h'] = header.encode()
    outputs['png-metadata.json'] = (json.dumps({
        'generator': 'tools/generate_color_fixtures.py',
        'precision': 80,
        'model': 'Original uint32 cHRM and integer samples normalized to F32; '
                 'independent power, source primary solve, Bradford to D65, '
                 'final linear F32; distinct background transforms and linear '
                 'composition. Source byte projection is separate.',
        'fixtures': records}, indent=2) + '\n').encode()
    return outputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'tests/data/color-numeric')
    args = parser.parse_args()
    profile = icc_profile()
    pixels = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 128, 128, 128])
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 1, 8, 2, 0, 0, 0))
           + chunk(b'iCCP', b'Independent P3\0\0' + zlib.compress(profile, 9))
           + chunk(b'IDAT', zlib.compress(b'\0' + pixels, 9))
           + chunk(b'IEND', b''))
    samples = []
    for name, rgb in [('red', [1, 0, 0]), ('green', [0, 1, 0]),
                      ('blue', [0, 0, 1]), ('white', [1, 1, 1]),
                      ('black', [0, 0, 0]), ('gray', ['0.5'] * 3),
                      ('interior', ['0.4', '0.5', '0.6']),
                      ('extended', ['-0.2', '1.2', '0.1'])]:
        rgb = [D(x) for x in rgb]
        xyz = apply(P3, list(map(decode, rgb)))
        linear = apply(inverse(SRGB), xyz)
        wire = [int((min(D(1), max(D(0), v)) * 100 + D('.5')))
                for v in rgb]
        samples.append({'name': name, 'p3': list(map(float, rgb)),
                        'xyz_d65': list(map(float, xyz)),
                        'xyz_d50': list(map(float, adapt(xyz))),
                        'linear_srgb': list(map(float, linear)),
                        'wire_percentage': wire,
                        'decoded_rgb8': [(v * 255 + 50) // 100 for v in wire]})
    opponent_samples = []
    for value in ([D(str(v)) for v in sample['linear_srgb']]
                  for sample in samples):
        oklab, lab, din = opponents(value)
        opponent_samples.append({'linear': list(map(float, value)),
                                 'oklab': oklab, 'cielab': lab, 'din99d': din})
    for value in ([D('-.1')] * 3, [D('1.2')] * 3):
        oklab, lab, din = opponents(value)
        opponent_samples.append({'linear': list(map(float, value)),
                                 'oklab': oklab, 'cielab': lab, 'din99d': din})
    trc = [{'input': float(D(s)), 'decoded': float(decode(D(s))),
            'encoded': float(encode(D(s)))}
           for s in ['-1.2', '-0.2', '-0.04045001', '-0.04045',
                     '-0.04044999', '-0.00313081', '-0.0031308',
                     '-0.00313079', '0', '0.00313079', '0.0031308',
                     '0.00313081', '0.04044999', '0.04045',
                     '0.04045001', '0.5', '1', '1.2']]
    manifest = {'schema': 1, 'generator': 'tools/generate_color_fixtures.py',
                'arithmetic': '60-digit Decimal; chromaticities solved independently',
                'sources': ['https://registry.color.org/rgb-registry/displayp3',
                            'https://www.color.org/specification/ICC.1-2022-05.pdf',
                            'https://www.w3.org/TR/css-color-4/#color-conversion-code'],
                'input': {'file': 'display-p3.png', 'samples_rgb8': list(pixels),
                          'metadata': 'iCCP only', 'profile': 'display-p3.icc',
                          'profile_sha256': hashlib.sha256(profile).hexdigest(),
                          'png_sha256': hashlib.sha256(png).hexdigest()},
                'tolerances': {'trc_double': 1e-12, 'xyz_double': 1e-10,
                               'p3_linear_roundtrip': 1e-5,
                               'icc_xyz': 2e-4},
                'samples': samples, 'trc': trc, 'opponents': opponent_samples}
    banner = '/* Generated by tools/generate_color_fixtures.py. */\n'
    header = banner + 'static double const color_vectors[][12] = {\n'
    for s in samples:
        header += '    /* ' + s['name'] + ' */\n    {\n'
        for key in ('p3', 'xyz_d65', 'xyz_d50', 'linear_srgb'):
            header += '        ' + fmt(s[key]) + ',\n'
        header += '    },\n'
    header += '};\n'
    trc_header = banner + 'static double const color_trc[][3] = {\n'
    for sample in trc:
        trc_header += '    {\n        ' + fmt(sample.values()) + '\n    },\n'
    trc_header += '};\n'
    opponent_header = banner + 'static double const color_opponents[][12] = {\n'
    for sample in opponent_samples:
        opponent_header += '    {\n'
        for key in ('linear', 'oklab', 'cielab', 'din99d'):
            opponent_header += '        ' + fmt(sample[key]) + ',\n'
        opponent_header += '    },\n'
    opponent_header += '};\n'
    icc_header = banner + 'static unsigned char const color_icc[] = {\n'
    for offset in range(0, len(profile), 12):
        icc_header += '    ' + ', '.join(f'0x{v:02x}' for v in profile[offset:offset+12]) + ',\n'
    icc_header += '};\n'
    lut_outputs = {}
    for name, reverse in [('forward-lut', False), ('paired-lut', True)]:
        data = lut_profile(reverse)
        symbol = 'color_' + name.replace('-', '_')
        table = banner + f'static unsigned char const {symbol}[] = {{\n'
        for offset in range(0, len(data), 12):
            table += '    ' + ', '.join(f'0x{v:02x}' for v in data[offset:offset+12]) + ',\n'
        table += '};\n'
        lut_outputs[name + '.icc'] = data
        lut_outputs[name + '.h'] = table.encode()
    manifest['lut_profiles'] = {'forward-lut.icc': 'A2B0 only',
        'paired-lut.icc': 'A2B0 and B2A0, intentionally non-invertible',
        'semantics': 'Raw selected LUT payload; no additional black-point compensation',
        'expected_xyz_d50': [0.25, 0.5, 0.75],
        'expected_device_rgb': [0.2, 0.4, 0.6]}
    outputs = {'icc.h': icc_header.encode(), 'opponents.h': opponent_header.encode(),'manifest.json': (json.dumps(manifest, indent=2) + '\n').encode(),
               'vectors.h': header.encode(), 'trc.h': trc_header.encode(),
               'display-p3.icc': profile, 'display-p3.png': png}
    outputs.update(lut_outputs)
    outputs.update(png_sample_oracles(profile))
    outputs.update(legacy_png_icc_oracle())
    outputs.update(frame_byte_oracles())
    outputs.update(png_byte_target_oracles())
    outputs.update(png_metadata_oracles())
    outputs.update(cmyk_sample_oracles())
    outputs.update(gray_sample_oracles())
    power_outputs = rgb_power_oracles()
    outputs.update(power_outputs)
    outputs.update(png_palette_oracles(power_outputs['rgb-power.json']))
    from generate_png_chrm_fixtures import build as png_chrm_fixtures
    outputs.update(png_chrm_fixtures(outputs['palette-trns-cms.png']))
    from generate_png_arithmetic_tests import build as png_arithmetic_fixtures
    outputs.update(png_arithmetic_fixtures())
    outputs.update(jpeg_sample_oracles())
    outputs.update(cli_rgb_oracles())
    outputs.update(hdr_sample_oracles())
    outputs.update(palette_export_oracles())
    # Keep the exact M0 parser probes, explicitly outside conformance and
    # color accuracy claims. Complete profiles own the numeric tests above.
    minimal_header = banner
    minimal_records = []
    for name, data in [('display-p3', icc_profile(minimal=True)),
                       ('forward-lut', lut_profile(False, minimal=True)),
                       ('paired-lut', lut_profile(True, minimal=True))]:
        filename = 'minimal-' + name + '.icc'
        symbol = 'color_minimal_' + name.replace('-', '_')
        minimal_header += f'static unsigned char const {symbol}[] = {{\n'
        for offset in range(0, len(data), 12):
            minimal_header += '    ' + ', '.join(
                f'0x{v:02x}' for v in data[offset:offset+12]) + ',\n'
        minimal_header += '};\n'
        outputs[filename] = data
        minimal_records.append({'file': filename,
            'sha256': hashlib.sha256(data).hexdigest(),
            'purpose': 'M0 byte-identical builtin parser acceptance only'})
    outputs['minimal-profiles.h'] = minimal_header.encode()
    manifest['minimal_profiles'] = minimal_records
    manifest['profile_structure'] = {
        'display-p3.icc': 'mntr: matrix/TRC + wtpt, chad, desc, cprt',
        'forward-lut.icc': 'scnr: A2B0 + wtpt, desc, cprt',
        'paired-lut.icc': 'mntr: A2B0, B2A0 + wtpt, desc, cprt',
        'change': 'Required metadata/class only; numeric matrix/TRC/LUT unchanged'}
    outputs['manifest.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    if not args.check:
        args.output.mkdir(parents=True, exist_ok=True)
    for name, content in outputs.items():
        if name.endswith('.h'):
            guard = 'COLOR_FIXTURE_' + name[:-2].upper().replace('-', '_')
            content = (f'#ifndef {guard}\n#define {guard}\n'.encode() +
                       content + b'#endif\n')
        path = args.output / name
        if args.check:
            if path.read_bytes() != content:
                raise SystemExit(f'stale fixture: {path}')
        else:
            path.write_bytes(content)
    print('Independent color fixtures verified' if args.check else
          'Independent color fixtures generated')


if __name__ == '__main__':
    main()
