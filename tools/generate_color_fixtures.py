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


def icc_profile():
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
    return pack_profile(tags)


def pack_profile(tags):
    header = bytearray(128)
    header[8:12] = bytes([4, 0x30, 0, 0])
    header[12:24] = b'mntrRGB XYZ '
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


def lut_profile(reverse):
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
    return pack_profile(tags)


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
                'icc_linear': [], 'srgb_linear': []}
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
            for variant in ('p3', 'raw', 'p3-oriented', 'p3-background'):
                metadata = b''
                if variant != 'raw':
                    metadata += chunk(b'iCCP', b'Independent P3\0\0'
                                      + zlib.compress(profile, 9))
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
                    'definition': 'display-p3.icc' if variant != 'raw' else None,
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
