#!/usr/bin/env python3
"""Independent FS/soft-clamp work-band index fixtures, without libsixel calls.

Compare 50-digit arithmetic with binary32 rounding at each stored operation.
The frozen indexes agree despite those intermediate rounding differences.
"""
import argparse
from decimal import Decimal as D, localcontext
from pathlib import Path
import struct

WIDTH, HEIGHT = 8, 17
PALETTE = [[-.25, .125, .875], [.5, .5, .5], [1.25, .875, .125]]
CONFIGS = [(6, 2, False), (6, 1, True), (6, 2, False), (12, 6, True)]


def binary32(value):
    return D(struct.unpack('f', struct.pack('f', float(value)))[0])


def solve(band, overlap, serpentine, rounded=False, clamp=True,
          local_parity=False, byte_palette=False):
    source = [[D((n * 37 + 79) % 193 - 32) / 128
               for n in range(3 * pixel, 3 * pixel + 3)]
              for pixel in range(WIDTH * HEIGHT)]
    palette = [[D(value) for value in row] for row in PALETTE]
    if byte_palette:
        palette = [[D(round(max(0, min(1, float(value))) * 255)) / 255
                    for value in row] for row in palette]
    output = [None] * (WIDTH * HEIGHT)
    minimum_gap, limited = D('Infinity'), 0
    store = binary32 if rounded else lambda value: value
    for body_start in range(0, HEIGHT, band):
        first = max(0, body_start - overlap)
        last = min(HEIGHT, body_start + band)
        working = [row[:] for row in source[first * WIDTH:last * WIDTH]]
        for y in range(first, last):
            scan_y = y - first if local_parity else y
            direction = -1 if serpentine and scan_y % 2 else 1
            columns = range(WIDTH) if direction == 1 else range(WIDTH-1, -1, -1)
            for x in columns:
                index = (y - first) * WIDTH + x
                original = source[y * WIDTH + x]
                candidate = working[index][:]
                correction = [value - start
                              for value, start in zip(candidate, original)]
                ratio = D(1)
                if clamp:
                    for start, delta in zip(original, correction):
                        lower = min(D(0), start) - D('.01')
                        upper = max(D(1), start) + D('.01')
                        if delta > 0:
                            ratio = min(ratio, (upper - start) / delta)
                        if delta < 0:
                            ratio = min(ratio, (lower - start) / delta)
                    if ratio < 1:
                        limited += 1
                        candidate = [store(start + ratio * delta)
                                     for start, delta in zip(original, correction)]
                distances = [sum((value - color)**2
                                 for value, color in zip(candidate, row))
                             for row in palette]
                order = sorted(range(3), key=lambda i: distances[i])
                chosen = order[0]
                minimum_gap = min(minimum_gap,
                                  distances[order[1]] - distances[order[0]])
                if y >= body_start:
                    output[y * WIDTH + x] = chosen
                # FS sends limited residuals to unvisited neighbors. A band
                # has its own mutable buffer and immutable original samples.
                taps = [(direction, 0, 7), (-direction, 1, 3),
                        (0, 1, 5), (direction, 1, 1)]
                for dx, dy, weight in taps:
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < WIDTH and yy < last:
                        dest = (yy - first) * WIDTH + xx
                        working[dest] = [
                            store(value + store(store(query - color) * weight / 16))
                            for value, query, color in
                            zip(working[dest], candidate, palette[chosen])]
    return output, float(minimum_gap), limited


def build():
    rows = []
    with localcontext() as context:
        context.prec = 50
        for band, overlap, serpentine in CONFIGS:
            expected, gap, limited = solve(band, overlap, serpentine)
            rounded = solve(band, overlap, serpentine, rounded=True)[0]
            assert expected == rounded
            assert gap > .0017 and limited > 0
            # These deliberately different recurrences must fail the witness.
            controls = [solve(HEIGHT, 0, serpentine)[0],
                        solve(band, 0, serpentine)[0],
                        solve(band, overlap, serpentine, clamp=False)[0],
                        solve(band, overlap, serpentine, byte_palette=True)[0]]
            assert all(expected != control for control in controls)
            if overlap == 1:
                assert expected != solve(band, overlap, serpentine,
                                         local_parity=True)[0]
            if band == 6:
                assert expected != solve(2, overlap, serpentine)[0]
            rows.append((expected, gap, limited))
    output = ['/* Independent oracle: generate_clamp_band_fixtures.py. */',
              '#ifndef COLOR_CLAMP_BANDS_H', '#define COLOR_CLAMP_BANDS_H',
              'static char const *const clamp_band_indexes[4] = {']
    for expected, gap, limited in rows:
        output.append('    /* Min squared-distance gap %.7g; %d limited pixels. */'
                      % (gap, limited))
        for y in range(HEIGHT):
            output.append('    "' + ''.join(str(n) for n in expected[y*WIDTH:(y+1)*WIDTH])
                          + '"' + (',' if y == HEIGHT-1 else ''))
    return '\n'.join(output + ['};', '#endif', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = (Path(__file__).resolve().parents[1] /
            'tests/data/color-numeric/clamp-bands.h')
    output = build()
    if args.check:
        if path.read_text() != output:
            parser.exit(1, 'stale clamp work-band indexes\n')
    else:
        path.write_text(output)


if __name__ == '__main__':
    main()
