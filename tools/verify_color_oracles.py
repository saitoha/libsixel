#!/usr/bin/env python3
"""Independently audit stored PNG certificates and UINT16 source rounding.

This development tool does not invoke a decoder, CMS, fixture generator,
or encoder. Runtime pixel assertions remain owned by tests/.
"""

import argparse
from fractions import Fraction as Q
import json
from pathlib import Path
import struct
import sys


def require(condition, label):
    if not condition:
        raise ValueError(label)


def multiply(left, right):
    return [[sum(x * y for x, y in zip(row, col))
             for col in zip(*right)] for row in left]


def determinant(a):
    return (a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
            - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
            + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]))


def inverse(a):
    result = []
    divisor = determinant(a)
    require(divisor != 0, "singular oracle matrix")
    for row in range(3):
        output = []
        for col in range(3):
            minor = [[a[r][c] for c in range(3) if c != row]
                     for r in range(3) if r != col]
            output.append((-1) ** (row + col)
                          * (minor[0][0] * minor[1][1]
                             - minor[0][1] * minor[1][0]) / divisor)
        result.append(output)
    require(multiply(a, result) == [[int(i == j) for j in range(3)]
                                   for i in range(3)], "inverse identity")
    return result


def white(grid):
    return [[Q(grid[0], grid[1])], [Q(1)],
            [Q(100000 - grid[0] - grid[1], grid[1])]]


def source_matrix(grid):
    # Use Cramer's rule rather than the fixture generator's Gaussian solve.
    primaries = [[Q(grid[2*c+2], grid[2*c+3]) for c in range(3)],
                 [Q(1)] * 3,
                 [Q(100000-grid[2*c+2]-grid[2*c+3], grid[2*c+3])
                  for c in range(3)]]
    target = white(grid)
    divisor = determinant(primaries)
    require(divisor != 0, "singular source primaries")
    scales = []
    for col in range(3):
        replaced = [[target[r][0] if c == col else primaries[r][c]
                     for c in range(3)] for r in range(3)]
        scales.append(determinant(replaced) / divisor)
    return [[primaries[r][c] * scales[c] for c in range(3)]
            for r in range(3)]


def absolute(matrix):
    return [[abs(value) for value in row] for row in matrix]


def exact_float(word):
    """Decode finite binary32 bits using integers only."""
    exponent = (word >> 23) & 255
    require(exponent != 255, "nonfinite oracle value")
    mantissa = word & 0x7fffff
    if exponent:
        mantissa += 1 << 23
    power = exponent - 150 if exponent else -149
    value = (Q(mantissa << power) if power >= 0
             else Q(mantissa, 1 << -power))
    return -value if word & 0x80000000 else value


def rounded(value):
    """Find adjacent binary32 values, then compare exact rational distances."""
    sign = 0x80000000 if value < 0 else 0
    value = abs(value)
    low, high = 0, 0x7f7fffff
    require(value <= exact_float(high), "oracle outside finite binary32")
    while low < high:
        middle = (low + high + 1) // 2
        if exact_float(middle) <= value:
            low = middle
        else:
            high = middle - 1
    if exact_float(low) == value:
        return sign | low
    below = value - exact_float(low)
    above = exact_float(low + 1) - value
    choice = low if below < above or (below == above and low % 2 == 0) else low + 1
    return sign | choice


def verify_png(path):
    srgb = [31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000]
    bradford = [[Q(v, 10000) for v in row] for row in
                ((8951, 2664, -1614), (-7502, 17135, 367), (389, -685, 10296))]
    target = inverse(source_matrix(srgb))
    bradford_inverse = inverse(bradford)
    unit = Q(1, 2**53)
    gamma = 80 * unit / (1 - 80 * unit)
    certificates = json.loads(path.read_text())
    counts = {}
    for category, entries in certificates.items():
        for index, entry in enumerate(entries):
            label = f"{category}[{index}]"
            grid, values = entry['grid'], entry['input_bits']
            matrix = source_matrix(grid)
            expanded = absolute(matrix)
            if grid[:2] != srgb[:2]:
                destination = multiply(bradford, white(srgb))
                origin = multiply(bradford, white(grid))
                diagonal = [[destination[r][0] / origin[r][0]
                             if r == c else Q(0) for c in range(3)]
                            for r in range(3)]
                matrix = multiply(bradford_inverse,
                                  multiply(diagonal, multiply(bradford, matrix)))
                expanded = multiply(absolute(bradford_inverse),
                                    multiply(absolute(diagonal),
                                             multiply(absolute(bradford), expanded)))
            matrix = multiply(target, matrix)
            expanded = multiply(absolute(target), expanded)
            exact = [sum(x * exact_float(y) for x, y in zip(row, values))
                     for row in matrix]
            bounds = [gamma * sum(x * abs(exact_float(y))
                                 for x, y in zip(row, values))
                      for row in expanded]
            require(exact == list(map(Q, entry['exact_values'])), label + " exact")
            require(bounds == list(map(Q, entry['absolute_forward_bounds'])),
                    label + " forward bound")
            expected = entry['expected_bits']
            require(list(map(rounded, exact)) == expected, label + " rounding")
            require([rounded(x-e) for x, e in zip(exact, bounds)] == expected,
                    label + " lower certificate endpoint")
            require([rounded(x+e) for x, e in zip(exact, bounds)] == expected,
                    label + " upper certificate endpoint")
            if category == 'loader_tiny':
                samples = entry['sample']
                require([rounded(Q(n, 65535)**10) for n in samples] == values,
                        label + " original integer transfer")
                require([rounded(exact_float(rounded(Q(n, 65535)))**10)
                         for n in samples] == values,
                        label + " normalized float transfer")
        counts[category] = len(entries)
    require(sum(counts.values()) > 0, "empty PNG certificate inventory")
    return counts


def float_word(value):
    return struct.unpack('>I', struct.pack('>f', value))[0]


def verify_uint16(path):
    require(sys.float_info.radix == 2 and sys.float_info.mant_dig == 53,
            "UINT16 audit requires binary64 host arithmetic")
    words = []
    for numerator in range(65536):
        word = float_word(numerator / 65535.0)
        target = Q(numerator, 65535)
        error = abs(exact_float(word) - target)
        for adjacent in (word - 1, word + 1):
            if adjacent < 0:
                continue
            other = abs(exact_float(adjacent) - target)
            require(error < other or (error == other and word % 2 == 0),
                    f"UINT16 nearest-even normalization: {numerator}")
        words.append(word)
    for numerator in range(65536):
        require(float_word(1.0 - numerator / 65535.0) == words[65535-numerator],
                f"UINT16 MINISWHITE polarity: {numerator}")
    fixtures = json.loads(path.read_text())['fixtures']
    for item in fixtures:
        # Invert the displayed coordinates instead of reusing the generator's
        # source-to-destination placement function.
        require(item['orientation'] in (1, 2, 3, 4), "separate16 orientation")
        expected = []
        for row in range(2):
            for col in range(3):
                x = 2-col if item['orientation'] in (2, 3) else col
                y = 1-row if item['orientation'] in (3, 4) else row
                pixel = item['samples'][y*3+x]
                rgb = (pixel if item['photo'] == 2 else
                       [65535-pixel[0] if item['photo'] == 0 else pixel[0]] * 3)
                expected.extend(f'{words[n]:08x}' for n in rgb)
        require(expected == item['source_words'], item['name'] + " source bits")
    require(len(fixtures) > 0, "empty separate16 fixture inventory")
    return {'normalized_values': 65536, 'polarity_values': 65536,
            'fixture_tables': len(fixtures)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind', choices=('all', 'png', 'uint16'), default='all')
    parser.add_argument('--source-root', type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument('--png-certificates', type=Path)
    parser.add_argument('--tiff-fixtures', type=Path)
    args = parser.parse_args()
    data = args.source_root / 'tests/data/color-numeric'
    result = {}
    try:
        if args.kind in ('all', 'png'):
            result['png'] = verify_png(args.png_certificates or
                                      data / 'png-arithmetic-certificates.json')
        if args.kind in ('all', 'uint16'):
            result['uint16'] = verify_uint16(args.tiff_fixtures or
                                            data / 'tiff-separate16.json')
    except (OSError, ValueError, KeyError, TypeError, ZeroDivisionError) as error:
        parser.exit(1, f'color oracle audit failed: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
