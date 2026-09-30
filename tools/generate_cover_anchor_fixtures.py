#!/usr/bin/env python3
"""Generate nominal sRGB hard-cover anchors from independent Decimal equations."""
import argparse
from decimal import Decimal as D
from pathlib import Path
import generate_color_fixtures as ref


def build():
    # Policy order is corners, faces, edges; 128 is a byte code, not 0.5.
    corners = [(0, 0, 0), (255, 0, 0), (0, 255, 0), (0, 0, 255),
               (255, 255, 0), (255, 0, 255), (0, 255, 255), (255, 255, 255)]
    faces = [tuple(v if c == axis else 128 for c in range(3))
             for axis in range(3) for v in (0, 255)]
    edges = []
    for axis in range(3):
        for b in (0, 255):
            for a in (0, 255):
                tail = iter((a, b))
                edges.append(tuple(128 if c == axis else next(tail)
                                   for c in range(3)))
    rows = []
    for rgb in corners + faces + edges + [(63.75,)*3, (67.734375,)*3, (65.7421875,)*3]:
        gamma = [D(c)/255 for c in rgb]
        linear = list(map(ref.decode, gamma))
        rows.append([gamma, linear, *ref.opponents(linear)])
    out = ['/* Independent 60-digit oracle: generate_cover_anchor_fixtures.py. */',
           '#ifndef COLOR_COVER_ANCHORS_H', '#define COLOR_COVER_ANCHORS_H',
           'static double const color_cover_anchors[5][29][3] = {']
    for space, name in enumerate(('gamma', 'linear', 'oklab', 'cielab', 'din99d')):
        out.append('    { /* '+name+' */')
        for row in rows:
            # Nine significant digits exceed the float32 observation budget.
            out.append('        { '+', '.join(format(float(v), '.9g')
                                              for v in row[space])+' },')
        out.append('    },')
    return '\n'.join(out+['};', '#endif', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1]/'tests/data/color-numeric/cover-anchors.h'
    output = build()
    if args.check:
        if path.read_text() != output:
            parser.exit(1, 'stale hard-cover anchors\n')
    else:
        path.write_text(output)


if __name__ == '__main__':
    main()
