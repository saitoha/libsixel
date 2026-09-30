#!/usr/bin/env python3
"""Independent unsnapped working entries and legacy output wire codes."""
import argparse
from decimal import Decimal as D
from pathlib import Path
import generate_palette_space_fixtures as space_ref
import generate_snap_native_fixtures as snap_ref


def build():
    sources = [[D('-.25'), D('1.25'), D('98.125') / 255],
               [D('.351'), D('.533'), D('.712')]]
    rows = []
    for space, name in enumerate(space_ref.NAMES):
        entries = [list(map(space_ref.f32, snap_ref.typed(rgb, space)))
                   for rgb in sources]
        codes = [[snap_ref.legacy_wire(rgb, name, output) for rgb in entries]
                 for output in [0, 1]]
        codes.append([space_ref.wire(rgb, name, False)[0] for rgb in entries])
        rows.append((entries, codes))
    out = ['/* Independent oracle: generate_output_native_fixtures.py. */',
           '#ifndef COLOR_OUTPUT_NATIVE_H', '#define COLOR_OUTPUT_NATIVE_H',
           'static struct {', '    double entries[2][3];',
           '    unsigned char wire[3][2][3];',
           '} const output_native[5] = {']
    for name, (entries, outputs) in zip(space_ref.NAMES, rows):
        out += ['    { /* '+name+' */', '        {']
        for entry in entries:
            out += ['            {']
            out += ['                '+space_ref.literal(v)+',' for v in entry]
            out += ['            },']
        out += ['        },', '        {']
        for codes in outputs:
            out += ['            { '+', '.join('{ '+', '.join(map(str, v))+' }'
                                              for v in codes)+' },']
        out += ['        },', '    },']
    # Fixed-map values start as sRGB8 and bypass generated-palette snapping.
    rgb = [[D(v) / 255 for v in row] for row in
           [[88, 88, 88], [94, 94, 94], [88, 88, 88], [255, 0, 0]]]
    out += ['};', 'static unsigned char const output_map_wire[2][4][3] = {']
    for output in [0, 1]:
        codes = [snap_ref.legacy_wire(list(map(space_ref.f32, row)),
                                      'gamma', output) for row in rgb]
        out += ['    {']
        out += ['        { '+', '.join(map(str, row))+' },' for row in codes]
        out += ['    },']
    return '\n'.join(out+['};', '#endif', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1] / 'tests/data/color-numeric/output-native.h'
    text = build()
    if args.check:
        if path.read_text() != text:
            parser.exit(1, 'stale output-native fixture\n')
    else:
        path.write_text(text)


if __name__ == '__main__':
    main()
