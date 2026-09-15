#!/usr/bin/env python3
"""Generate literal RGB16 TIFFs and ICC-byte oracles without a decoder/CMM."""

from decimal import Decimal as D, localcontext
import hashlib
import json
import struct


def apply(matrix, vector):
    return [sum(x * y for x, y in zip(row, vector)) for row in matrix]


def inverse(matrix):
    """Use cofactors independently of production matrix elimination."""
    cofactors = []
    for i in range(3):
        row = []
        for j in range(3):
            minor = [[matrix[y][x] for x in range(3) if x != j]
                     for y in range(3) if y != i]
            row.append((minor[0][0] * minor[1][1]
                        - minor[0][1] * minor[1][0]) * (-1) ** (i + j))
        cofactors.append(row)
    determinant = sum(matrix[0][j] * cofactors[0][j] for j in range(3))
    return [[cofactors[j][i] / determinant for j in range(3)]
            for i in range(3)]


def destination_matrix():
    def white(x, y):
        x, y = D(x), D(y)
        return [x / y, D(1), (1 - x - y) / y]

    d65 = white('.3127', '.3290')
    d50 = [D('.9642'), D(1), D('.8249')]
    primaries = [white('.64', '.33'), white('.30', '.60'), white('.15', '.06')]
    basis = list(map(list, zip(*primaries)))
    weights = apply(inverse(basis), d65)
    rgb = [[x * weights[j] for j, x in enumerate(row)] for row in basis]
    bradford = [[D(x) for x in row] for row in [
        ['.8951', '.2664', '-.1614'], ['-.7502', '1.7135', '.0367'],
        ['.0389', '-.0685', '1.0296']]]
    source, target = apply(bradford, d65), apply(bradford, d50)
    columns = [apply(inverse(bradford), [v * t / s for v, t, s in
               zip(apply(bradford, column), target, source)])
               for column in zip(*rgb)]
    return list(map(list, zip(*columns)))


def tags(profile):
    result = {}
    for i in range(struct.unpack_from('>I', profile, 128)[0]):
        name, offset, size = struct.unpack_from('>4sII', profile, 132 + i * 12)
        result[name] = profile[offset:offset + size]
    return result


def srgb_profile(profile, destination):
    """Keep the original TRC/white/adaptation tags; replace primaries/name."""
    entries = tags(profile)
    for name, column in zip([b'rXYZ', b'gXYZ', b'bXYZ'], zip(*destination)):
        entries[name] = b'XYZ ' + bytes(4) + b''.join(struct.pack(
            '>i', int((x * 65536).to_integral_value())) for x in column)
    title = 'Independent sRGB'.encode('utf-16-be')
    entries[b'desc'] = (b'mluc' + bytes(4) + struct.pack('>II', 1, 12)
                        + b'enUS' + struct.pack('>II', len(title), 28) + title)
    offset = 132 + 12 * len(entries)
    table, payload = bytearray(), bytearray()
    for name, value in entries.items():
        table.extend(name + struct.pack('>II', offset + len(payload), len(value)))
        payload.extend(value + bytes(-len(value) % 4))
    header = bytearray(profile[:128])
    header[:4] = struct.pack('>I', offset + len(payload))
    header[84:100] = bytes(16)
    return bytes(header) + struct.pack('>I', len(entries)) + table + payload


def tiff(samples, profile, width=6, height=1, orientation=None, sampleformat=None):
    """Serialize one uncompressed contiguous strip, with literal UINT16 words.

    The default directory/payload layout preserves the original six-pixel
    audit fixture byte for byte; optional tags form separately named inputs.
    """
    raster = struct.pack('<18H', *sum(samples, []))
    entries = {256: (4, [width]), 257: (4, [height]), 258: (3, [16] * 3),
               259: (3, [1]), 262: (3, [2]), 273: (4, [0]),
               277: (3, [3]), 278: (4, [height]), 279: (4, [len(raster)]),
               284: (3, [1])}
    if profile is not None:
        entries[34675] = (7, profile)
    if orientation is not None:
        entries[274] = (3, [orientation])
    if sampleformat is not None:
        entries[339] = (3, [sampleformat] * 3)
    offset = 8 + 2 + len(entries) * 12 + 4
    table, payload = bytearray(), bytearray()
    for tag, (kind, value) in sorted(entries.items()):
        data = value if kind == 7 else struct.pack(
            '<' + ('H' if kind == 3 else 'I') * len(value), *value)
        encoded = (data.ljust(4, b'\0') if len(data) <= 4 else
                   struct.pack('<I', offset + len(payload)))
        if len(data) > 4:
            payload.extend(data)
        if tag == 273:
            strip_offset = 8 + 2 + len(table) + 8
        table.extend(struct.pack('<HHI', tag, kind, len(value)) + encoded)
    out = bytearray(b'II*\0' + struct.pack('<IH', 8, len(entries))
                    + table + bytes(4) + payload)
    struct.pack_into('<I', out, strip_offset, len(out))
    return bytes(out) + raster


def f32(value):
    return struct.unpack('>f', struct.pack('>f', float(value)))[0]


def expected(profile, samples, destination_inverse):
    entries = tags(profile)

    def fixed(blob, offset):
        return D(struct.unpack_from('>i', blob, offset)[0]) / 65536

    source_matrix = [[fixed(entries[c + b'XYZ'], 8 + i * 4)
                      for c in [b'r', b'g', b'b']] for i in range(3)]
    values = []
    for sample in samples:
        linear = []
        for channel, word in zip([b'r', b'g', b'b'], sample):
            x = D.from_float(f32(D(word) / 65535))
            curve = entries[channel + b'TRC']
            assert curve[:4] == b'para' and curve[8:10] == b'\0\4'
            g, a, b, c, d, e, f = [fixed(curve, 12 + i * 4) for i in range(7)]
            value = (a * x + b) ** g + e if x >= d else c * x + f
            linear.append(max(D(0), min(D(1), value)))
        values.append(apply(destination_inverse, apply(source_matrix, linear)))
    return values


def build(profile):
    with localcontext() as context:
        context.prec = 80
        return build_at_precision(profile)


def build_at_precision(profile):
    original = [[65535, 0, 0], [0, 65535, 0], [0, 0, 65535],
                [65535] * 3, [13107, 26214, 39321], [0] * 3]
    dark = [[v] * 3 for v in [1, 2, 16, 2650, 2651, 2652]]
    destination = destination_matrix()
    inv = inverse(destination)
    srgb = srgb_profile(profile, destination)
    linear = [expected(p, s, inv) for p, s in
              [(profile, original), (profile, dark), (srgb, original)]]
    specs = [('p3', profile, original, 6, 1, None, None, 0, 0),
             *[(f'p3-o{o}', profile, original, 3, 2, o, None, 0, 0)
               for o in [2, 3, 4]],
             ('p3-dark', profile, dark, 6, 1, None, None, 1, 1),
             ('p3-void', profile, original, 6, 1, None, 4, 0, 0),
             ('srgb', srgb, original, 6, 1, None, None, 0, 2),
             ('untagged', None, original, 6, 1, None, None, 0, 0),
             ('invalid', b'invalid ICC', original, 6, 1, None, None, 0, 0)]
    float_budget = D(64) / D(2) ** 24
    norm = max(sum(map(abs, row)) for row in inv)
    q = D(1) / 65536
    matrix_budget = norm * 4 * q / (1 - norm * 3 * q)
    native_budget = (1 + matrix_budget) * (1 + float_budget) - 1
    outputs, records = {}, []
    header = ('/* Generated from literal words and ICC tags; no CMM oracle. */\n'
              '#define COLOR_TIFF_SAMPLES 6\n'
              '#define COLOR_TIFF_FIXTURES 9\n'
              f'static double const color_tiff_float_relative = {float(float_budget):.17g};\n'
              f'static double const color_tiff_native_relative = {float(native_budget):.17g};\n'
              'static struct {\n    char const *file;\n'
              '    int width;\n    int height;\n    int orientation;\n'
              '    int source;\n    int linear;\n} const color_tiff_cases[] = {\n')
    for name, icc, samples, width, height, orientation, sf, source_id, linear_id in specs:
        filename = f'tiff-rgb16-{name}.tif'
        data = tiff(samples, icc, width, height, orientation, sf)
        outputs[filename] = data
        header += (f'    {{ "{filename}", {width}, {height}, '
                   f'{orientation or 1}, {source_id}, {linear_id} }},\n')
        records.append(dict(file=filename,sha256=hashlib.sha256(data).hexdigest(),
            width=width,height=height,orientation=orientation or 1,
            source_samples=samples,source_index=source_id,linear_index=linear_id,
            sampleformat=sf or 1,icc_sha256=hashlib.sha256(icc).hexdigest() if icc else None))
    header += '};\nstatic unsigned int const color_tiff_device[2][6][3] = {\n'
    for samples in [original, dark]:
        header += '    {\n'
        for sample in samples:
            bits = [struct.unpack('>I', struct.pack('>f', f32(D(v) / 65535)))[0]
                    for v in sample]
            header += '        { ' + ', '.join(f'0x{v:08x}u' for v in bits) + ' },\n'
        header += '    },\n'
    header += '};\nstatic double const color_tiff_linear[3][6][3] = {\n'
    for pixels in linear:
        header += '    {\n'
        for pixel in pixels:
            header += '        {\n'
            header += ''.join(f'            {f32(v):.17g},\n' for v in pixel)
            header += '        },\n'
        header += '    },\n'
    header += '};\n'
    outputs['tiff-rgb16.h'] = header.encode()
    outputs['tiff-rgb16.json'] = (json.dumps(dict(
        generator='tools/generate_tiff_fixtures.py',precision_decimal=80,
        oracle='Stored ICC s15Fixed16 tags, literal UINT16 -> binary32, cofactor inverse, Bradford, final binary32',
        fixtures=records,expected=[[list(map(str, pixel)) for pixel in image] for image in linear],
        expected_f32=[[[f32(v) for v in pixel] for pixel in image] for image in linear],
        error_contract=dict(float_relative=float(float_budget),native_relative=float(native_budget),
            float_formula='64 * 2^-24',matrix_quantum=str(q),matrix_budget=str(matrix_budget),
            inverse_infinity_norm=str(norm),native_formula='(1 + matrix_budget) * (1 + float_budget) - 1',
            comparison='abs(actual[c]-expected[c]) <= max(abs(expected[0..2])) * relative',
            black='exact zero; no absolute floor',scope='Existing test allowance, not a proved native error bound')),
        indent=2) + '\n').encode()
    return outputs
