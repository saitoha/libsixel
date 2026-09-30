#!/usr/bin/env python3
"""Independent typed built-in entries and destination RGB101 definitions."""
import argparse
from decimal import Decimal as D
from pathlib import Path
import generate_palette_space_fixtures as space_ref
import generate_snap_native_fixtures as snap_ref

# Literal public table contracts, not values read from the implementation.
XTERM16 = [[0,0,0], [128,0,0], [0,128,0], [128,128,0],
           [0,0,128], [128,0,128], [0,128,128], [192,192,192],
           [128,128,128], [255,0,0], [0,255,0], [255,255,0],
           [0,0,255], [255,0,255], [0,255,255], [255,255,255]]
TABLES = [XTERM16, [[v]*3 for v in [0,85,170,255]],
          [[v]*3 for v in [33,66,102,15,51,84,117,0]*2],
          [[0]*3, [255]*3]]


def array(value, indent=0):
    if not isinstance(value, list):
        return space_ref.literal(value)
    pieces = [array(v, indent+4) for v in value]
    inline = '{ '+', '.join(pieces)+' }'
    if '\n' not in inline and indent+len(inline) <= 78:
        return inline
    return '{\n'+' '*(indent+4)+(',\n'+' '*(indent+4)).join(pieces)+'\n'+' '*indent+'}'


def build():
    out = ['/* Independent oracle: generate_builtin_native_fixtures.py. */',
           '#ifndef COLOR_BUILTIN_NATIVE_H', '#define COLOR_BUILTIN_NATIVE_H',
           'static struct {', '    unsigned int count;',
           '    unsigned char rgb[16][3];', '    double entries[5][16][3];',
           '    unsigned char wire[5][4][16][3];',
           '} const builtin_native[4] = {']
    for table in TABLES:
        rgb = [[space_ref.f32(D(v)/255) for v in row] for row in table]
        entries = [[list(map(space_ref.f32, snap_ref.typed(row, space)))
                    for row in rgb] for space in range(5)]
        # The SMPTE-C witness uses gray2 only; unused slots are zero.
        wires = []
        for name, colors in zip(space_ref.NAMES, entries):
            wires.append([[space_ref.wire(c, name, False)[0] for c in colors],
                          [space_ref.wire(c, name, True)[0] for c in colors],
                          [snap_ref.legacy_wire(c, name, 0) for c in colors],
                          ([snap_ref.legacy_wire(c, name, 1) for c in colors]
                           if table is TABLES[1] else [[0,0,0] for c in colors])])
        out += ['    {', '        '+str(len(table))+',',
                '        '+array(table, 8)+',',
                '        '+array(entries, 8)+',',
                '        '+array(wires, 8)+',', '    },']
    return '\n'.join(out+['};', '#endif', ''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1] / 'tests/data/color-numeric/builtin-native.h'
    text = build()
    if args.check:
        if path.read_text() != text:
            parser.exit(1, 'stale builtin-native fixture\n')
    else:
        path.write_text(text)


if __name__ == '__main__':
    main()
