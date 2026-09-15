#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independent exact PNG arithmetic fixtures; no production matrix evaluator.

Gaussian elimination builds source RGB/XYZ and the standard sRGB inverse.
The final rational value is rounded directly to binary32, including ties.
"""
from fractions import Fraction as F
import math
import json
import struct
import zlib

N = 100000
SRGB = [31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000]
B = [[F(v, 10000) for v in row] for row in
     [[8951, 2664, -1614], [-7502, 17135, 367], [389, -685, 10296]]]
DECIMAL = [[F(v, 10**15) for v in row] for row in
           [[3240969941904521, -1537383177570093, -498610760293003],
            [-969243636280880, 1875967501507721, 41555057407176],
            [55630079696993, -203976958888977, 1056971514242878]]]


def mm(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(len(b)))
             for j in range(len(b[0]))] for i in range(len(a))]


def inverse(a):
    rows = [row[:] + [F(i == j) for j in range(3)] for i, row in enumerate(a)]
    for col in range(3):
        pivot = next((i for i in range(col, 3) if rows[i][col]), None)
        if pivot is None:
            raise ValueError('singular')
        rows[col], rows[pivot] = rows[pivot], rows[col]
        divisor = rows[col][col]
        rows[col] = [v / divisor for v in rows[col]]
        for i in range(3):
            if i == col:
                continue
            factor = rows[i][col]
            rows[i] = [a - factor * b for a, b in zip(rows[i], rows[col])]
    return [row[3:] for row in rows]


def white(grid):
    return [[F(grid[0], grid[1])], [F(1)],
            [F(N - grid[0] - grid[1], grid[1])]]


def source(grid):
    columns = [[F(grid[i], grid[i + 1]), F(1),
                F(N - grid[i] - grid[i + 1], grid[i + 1])]
               for i in range(2, 8, 2)]
    primaries = [[columns[c][r] for c in range(3)] for r in range(3)]
    scale = mm(inverse(primaries), white(grid))
    result = [[primaries[r][c] * scale[c][0] for c in range(3)]
              for r in range(3)]
    assert mm(result, [[F(1)]] * 3) == white(grid)
    return result


SRGB_SOURCE = source(SRGB)
STANDARD = inverse(SRGB_SOURCE)
IDENTITY = [[F(r == c) for c in range(3)] for r in range(3)]
assert mm(STANDARD, SRGB_SOURCE) == mm(SRGB_SOURCE, STANDARD) == IDENTITY
TARGETS = dict(decimal=DECIMAL,
               binary64=[[F(float(v)) for v in row] for row in DECIMAL],
               standard=STANDARD)


def integer_target(target):
    denominator = math.lcm(*(v.denominator for row in target for v in row))
    numerator = [[int(v * denominator) for v in row] for row in target]
    assert denominator < 2**63
    assert F(float(denominator)) == denominator
    assert all(abs(v) < 2**63 and F(float(v)) == v
               for row in numerator for v in row)
    return numerator, denominator


def model(grid, cat, target):
    if any(x < 0 or y <= 0 or x + y > N
           for x, y in zip(grid[::2], grid[1::2])):
        return 0, None
    try:
        result = source(grid)
    except ValueError:
        return 0, None
    if cat and grid[:2] != SRGB[:2]:
        cones = mm(B, white(grid))
        target_cones = mm(B, white(SRGB))
        if any(not row[0] for row in cones):
            return -1, None
        ratios = [[target_cones[r][0] / cones[r][0] if r == c else F(0)
                   for c in range(3)] for r in range(3)]
        adaptation = mm(inverse(B), mm(ratios, B))
        assert mm(adaptation, white(grid)) == white(SRGB)
        result = mm(adaptation, result)
    return 1, mm(target, result)


def value(bits):
    return struct.unpack('<f', struct.pack('<I', bits))[0]


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def rounded(value):
    # Direct rational ties-even rounding; no binary64 intermediate decision.
    if not value:
        return 0
    sign = 0x80000000 if value < 0 else 0
    value = abs(value)
    exponent = value.numerator.bit_length() - value.denominator.bit_length()
    scale = lambda n: F(2**n) if n >= 0 else F(1, 2**(-n))
    if value < scale(exponent):
        exponent -= 1
    step = max(exponent - 23, -149)
    scaled = value / scale(step)
    quotient, remainder = divmod(scaled.numerator, scaled.denominator)
    quotient += 2 * remainder > scaled.denominator or (
        2 * remainder == scaled.denominator and quotient % 2)
    if not quotient:
        return sign
    return bits(float(F(quotient) * scale(step))) | sign


def output(matrix, vector):
    x = [F(value(v)) for v in vector]
    exact = [sum(a * b for a, b in zip(row, x)) for row in matrix]
    return exact, list(map(rounded, exact))


def strings(matrix):
    return [[str(v) for v in row] for row in matrix]


def width(target):
    k, denominator = integer_target(target)
    b = [[int(v * 10000) for v in row] for row in B]
    adj = [[b[(c+1) % 3][(r+1) % 3] * b[(c+2) % 3][(r+2) % 3]
            - b[(c+1) % 3][(r+2) % 3] * b[(c+2) % 3][(r+1) % 3]
            for c in range(3)] for r in range(3)]
    det = sum(b[0][c] * adj[c][0] for c in range(3))
    qd = [sum(a * v for a, v in zip(row, [31270, 32900, 35830])) for row in b]
    qm = [N * max(map(abs, row)) for row in b]
    va = [[sum(abs(k[r][j] * adj[j][c]) for j in range(3))
           for c in range(3)] for r in range(3)]
    base = 32900 * denominator * abs(det)
    cat_d = N**2 * base * math.prod(qm)
    cat_n = max(N**2 * sum(va[r][c] * abs(qd[c]) * math.prod(qm)
                          for c in range(3)) for r in range(3))
    source_d = N**3 * denominator
    source_n = N**3 * max(sum(map(abs, row)) for row in k)
    row_bounds = [F(3 * N**2 * sum(va[r][c] * abs(qd[c]) * qm[c]
                                 for c in range(3)), base) for r in range(3)]
    source_bounds = [F(3 * N**3 * sum(map(abs, row)), denominator) for row in k]
    maximum_n = max(cat_n, source_n)
    maximum_d = max(cat_d, source_d)
    dot = 3 * maximum_n * 2**149
    midpoint = maximum_d * 2**223
    assert max(row_bounds + source_bounds) < 2**72
    assert max(2 * dot, midpoint) < 2**512
    assert math.ceil(maximum_d.bit_length() / 32) + 7 <= 16
    assert math.ceil(maximum_n.bit_length() / 32) + 5 <= 16
    gray = mm(target, white(SRGB))
    gray_error = max(abs(row[0] - 1) for row in gray)
    assert gray_error < F(1, 2**25)
    return dict(integer_numerators=k, common_denominator=denominator,
                source_numerator_bits=source_n.bit_length(),
                source_denominator_bits=source_d.bit_length(),
                cat_numerator_bits=cat_n.bit_length(), cat_denominator_bits=cat_d.bit_length(),
                dot_bits=dot.bit_length(), doubled_dot_bits=(2 * dot).bit_length(),
                midpoint_bits=midpoint.bit_length(), capacity_bits=512,
                row_l1_bounds=list(map(str, row_bounds)),
                source_row_l1_bounds=list(map(str, source_bounds)),
                gray_factors=strings(gray), gray_factor_max_error=str(gray_error),
                gray_f32_identity_certificate='max(abs(factor - 1)) < 2^-25',
                gray_exact_real_identity=gray == [[F(1)]] * 3)


def chunk(kind, payload):
    return (struct.pack('>I', len(payload)) + kind + payload
            + struct.pack('>I', zlib.crc32(kind + payload)))


def absolute(matrix):
    return [[abs(v) for v in row] for row in matrix]


def double_certificate(grid, vector):
    """Bound the ordinary expression before binary32 rounding, independently.

    All integer factors converted by the double constructor are below 2^53.
    Source/target/CAT ratios each introduce at most one division rounding.
    Expanding both three-term matrix sums and the final dot gives fewer than
    80 rounding factors along any product path. With u=2^-53, gamma(80)
    times the absolute expanded sum bounds the forward error, including
    cancellation. This uses only exact mathematical matrices and input bits;
    it neither reads production coefficients nor fits an observed error.
    """
    expanded = absolute(source(grid))
    if grid[:2] != SRGB[:2]:
        source_cones = mm(B, white(grid))
        target_cones = mm(B, white(SRGB))
        ratios = [[abs(target_cones[i][0] / source_cones[i][0])
                   if i == j else F(0) for j in range(3)] for i in range(3)]
        expanded = mm(absolute(inverse(B)),
                      mm(ratios, mm(absolute(B), expanded)))
    expanded = mm(absolute(STANDARD), expanded)
    valid, matrix = model(grid, 1, STANDARD)
    assert valid == 1
    exact, expected = output(matrix, vector)
    unit = F(1, 2**53)
    gamma = 80 * unit / (1 - 80 * unit)
    bounds = [gamma * sum(a * abs(F(value(v))) for a, v in zip(row, vector))
              for row in expanded]
    # Every point in the certified enclosure rounds to the same float32.
    assert all(rounded(y - error) == result == rounded(y + error)
               for y, error, result in zip(exact, bounds, expected))
    return dict(grid=grid, input_bits=vector, expected_bits=expected,
                exact_values=list(map(str, exact)),
                absolute_forward_bounds=list(map(str, bounds)),
                rounding_cell_is_singleton=True)


def hex_row(values):
    return '    { ' + ', '.join('0x%08xu' % v for v in values) + ' },'


def build():
    # This bound is derived from the input domain, not production coefficients.
    certificate = width(STANDARD)
    assert certificate['midpoint_bits'] <= 448
    outputs = {}
    certificates = dict(dispatch=[], tiny=[], loader_tiny=[])
    cases = [
        SRGB,
        [31270, 32900, 68000, 32000, 26500, 69000, 15000, 6000],
        [34570, 35850, 68000, 32000, 26500, 69000, 15000, 6000],
        [31270, 32900, 15000, 6000, 30000, 60000, 64000, 33000],
        [31270, 32900, 59522, 6, 58680, 7, 57839, 8],
        [14071, 2978, 59522, 6, 58680, 7, 57839, 8],
    ]
    vectors = [
        [0, 0, 0], [0x80000000, 0, 0], [bits(1)] * 3, [bits(.5)] * 3,
        [1] * 3, [bits(.5) + 1, bits(.5), bits(.5)],
        [bits(1), bits(2**-100), 0], [bits(.125), bits(.25), bits(.75)],
        [bits(824041 / 2097152), bits(3358041 / 16777216), 0],
        [bits(10083 / 65535), bits(5136 / 65535), 0],
    ]
    lines = [
        '/* SPDX-License-Identifier: MIT */',
        '/* Independent Fraction Gaussian solve and direct binary32 rounding.',
        ' * Generated by tools/generate_png_arithmetic_tests.py;',
        ' * do not hand-edit. */',
        '#include <stdint.h>', '#include <stddef.h>',
        'static int64_t const png_arith_grids[][8] = {',
    ]
    lines += ['    { ' + ', '.join(map(str, g)) + ' },' for g in cases]
    lines += ['};', 'static uint32_t const png_arith_inputs[][3] = {']
    lines += ['    { ' + ', '.join('UINT32_C(0x%08x)' % x for x in v)
              + ' },' for v in vectors]
    lines += ['};', 'static uint32_t const png_arith_expected[][2][10][3] = {']
    for grid in cases:
        lines += ['    {']
        for cat in [0, 1]:
            valid, matrix = model(grid, cat, STANDARD)
            assert valid == 1
            lines += ['        {']
            for vector in vectors:
                values = ', '.join('0x%08xu' % v
                                   for v in output(matrix, vector)[1])
                lines += ['            { ' + values + ' },']
            lines += ['        },']
        lines += ['    },']
    lines += ['};']
    dispatch_grids = [
        [31270,32900,20000,20000,21000,20001,20001,20100],
        [31270,32900,20000,20000,21000,20000,20000,20100],
        [31270,32900,20000,20000,21000,19999,20001,20100],
        [31270,32900,20000,20000,20001,20100,21000,20001],
        [31270,32900,20000,20000,20000,20100,21000,20000],
        [31270,32900,20000,20000,20001,20100,21000,19999],
        [14071,2978,*SRGB[2:]], [16256,5440,*SRGB[2:]],
        [49348,49240,*SRGB[2:]], cases[5],
        [49347,49241,*SRGB[2:]], [49347,49240,*SRGB[2:]],
        [16257,5440,*SRGB[2:]],
    ]
    dispatch_input = list(map(bits, [.125, .25, .75]))
    lines += ['static int64_t const png_arith_dispatch_grids[][8] = {']
    lines += ['    { ' + ', '.join(map(str, grid)) + ' },'
              for grid in dispatch_grids]
    lines += ['};', 'static uint32_t const png_arith_dispatch_expected[][3] = {']
    for grid in dispatch_grids:
        proof = double_certificate(grid, dispatch_input)
        certificates['dispatch'].append(proof)
        lines += [hex_row(proof['expected_bits'])]
    lines += ['};', 'static uint32_t const png_arith_tiny_inputs[][3] = {']
    tiny = [[bits(2**-100),0,0], [0,bits(2**-100),0], [1,0,0], [0,1,0],
            [bits(2**-100),bits(2**-101),0]]
    lines += [hex_row(vector) for vector in tiny]
    lines += ['};', 'static uint32_t const png_arith_tiny_expected[][5][3] = {']
    for grid in cases[1:3]:
        lines += ['    {']
        for vector in tiny:
            proof = double_certificate(grid, vector)
            certificates['tiny'].append(proof)
            lines += ['    ' + hex_row(proof['expected_bits'])]
        lines += ['    },']
    lines += ['};']
    samples = [
        (0, 0, 0), (65535, 65535, 65535), (32768, 32768, 32768),
        (10083, 5136, 0), (15393, 7841, 0), (16661, 8487, 0),
        (16777, 8546, 0), (65535, 0, 0),
    ]
    # Cases 6 and 7 exercise the same exact reference through APNG rectangles
    # and fully transparent RGBA with a colored file background, respectively.
    loader_cases = cases + [cases[5], cases[5], cases[1], cases[2]]
    tiny_samples = [(64,0,0), (0,64,0), (64,32,0), (32,64,0),
                    (128,0,0), (0,128,0), (32,0,0), (0,32,0)]
    for index, grid in enumerate(loader_cases):
        rgba = index == 7
        png = b'\x89PNG\r\n\x1a\n' + chunk(
            b'IHDR', struct.pack('>IIBBBBB', 8, 1, 16, 6 if rgba else 2,
                                 0, 0, 0))
        png += chunk(b'gAMA', struct.pack('>I', 10000 if index >= 8 else 100000))
        png += chunk(b'cHRM', struct.pack('>8I', *grid))
        if index == 6:
            png += chunk(b'acTL', struct.pack('>II', 1, 1))
            png += chunk(b'fcTL', struct.pack('>IIIIIHHBB',
                         0, 8, 1, 0, 0, 1, 100, 0, 0))
        if rgba:
            png += chunk(b'bKGD', struct.pack('>3H', *samples[3]))
            row = b'\0' + struct.pack('>4H', 0, 0, 0, 0) * 8
        else:
            row_samples = tiny_samples if index >= 8 else samples
            row = b'\0' + b''.join(struct.pack('>3H', *v) for v in row_samples)
        png += chunk(b'IDAT', zlib.compress(row)) + chunk(b'IEND', b'')
        outputs[f'png-arithmetic-{index}.png'] = png
        lines += [f'static unsigned char const png_arith_png_{index}[] = {{']
        for i in range(0, len(png), 12):
            lines += ['    ' + ', '.join('0x%02x' % v for v in png[i:i+12])
                      + ',']
        lines += ['};']
    lines += ['static struct { unsigned char const *bytes; size_t size; } const',
              'png_arith_png[] = {']
    lines += [f'    {{ png_arith_png_{i}, sizeof(png_arith_png_{i}) }},'
              for i in range(len(loader_cases))]
    lines += ['};', 'static uint32_t const png_arith_loader_expected[][24] = {']
    for index, grid in enumerate(loader_cases):
        valid, matrix = model(grid, 1, STANDARD)
        assert valid == 1
        input_samples = ([samples[3]] * 8 if index == 7 else
                         tiny_samples if index >= 8 else samples)
        values = []
        for sample in input_samples:
            vector = [rounded(F(x, 65535)) for x in sample]
            if index >= 8:
                # Static RGB16 stores normalized float32 before its TRC.
                # These chosen power-10 samples round identically even if
                # normalization is kept rational through the power operation.
                vector = [rounded(F(value(v))**10) for v in vector]
                assert vector == [rounded(F(x, 65535)**10) for x in sample]
                proof = double_certificate(grid, vector)
                certificates['loader_tiny'].append(dict(sample=list(sample),
                                                        **proof))
            values += output(matrix, vector)[1]
        lines += ['    {']
        for i in range(0, len(values), 4):
            lines += ['        ' + ', '.join('0x%08xu' % v
                                            for v in values[i:i+4]) + ',']
        lines += ['    },']
    lines += ['};', '']
    outputs['png-arithmetic.h'] = '\n'.join(lines).encode()
    outputs['png-arithmetic-certificates.json'] = (json.dumps(certificates,
        indent=2, sort_keys=True) + '\n').encode()
    return outputs


if __name__ == '__main__':
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / 'tests/data/color-numeric'
    errors = []
    for name, data in build().items():
        if name.endswith('.h'):
            guard = 'COLOR_FIXTURE_' + name[:-2].upper().replace('-', '_')
            data = (f'#ifndef {guard}\n#define {guard}\n'.encode()
                    + data + b'#endif\n')
        path = root / name
        if args.check:
            if not path.is_file() or path.read_bytes() != data:
                errors.append(name)
        else:
            path.write_bytes(data)
    if errors:
        parser.exit(1, 'stale PNG arithmetic fixtures: ' + ', '.join(errors))
