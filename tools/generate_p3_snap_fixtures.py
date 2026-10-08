#!/usr/bin/env python3
"""Derive P3 snap targets from chromaticities, without running libsixel."""
import argparse
from decimal import Decimal as D
from itertools import product
from pathlib import Path
import generate_color_fixtures as ref
import generate_palette_space_fixtures as space_ref


def typed(rgb, space):
    linear = ref.apply(ref.inverse(ref.SRGB),
                       ref.apply(ref.P3, list(map(ref.decode, rgb))))
    values = [list(map(ref.encode, linear)), linear,
              *ref.opponents(linear)][space]
    return [D(str(v)) for v in values]


def build():
    tones = [D((q*255+50)//100)/255 for q in range(101)]
    sources = [['1', '.05', '.15'], ['.351', '.533', '.712'],
               ['-.1', '1.05', '.4']]
    out = ['/* Independent oracle: generate_p3_snap_fixtures.py. */',
           '#if !defined(SIXEL_AMALGAMATION) || defined(BUILD_TEST_0001_GRID) \\',
           '    || defined(BUILD_TEST_0002_PIPELINE)',
           '#ifndef COLOR_P3_SNAP_H', '#define COLOR_P3_SNAP_H',
           '/* fixture, space, source/nearest/reversible, component. */',
           'static double const p3_snap_values[3][5][3][3] = {']
    codes = []
    wire_rows = []
    for rgb in sources:
        out.append('    {')
        rows = []
        wire_fixture = []
        for space, name in enumerate(space_ref.NAMES):
            source = list(map(space_ref.f32, typed(list(map(D, rgb)), space)))
            linear = space_ref.linear_from(source, name)
            restored = list(map(ref.encode, ref.apply(ref.inverse(ref.P3),
                                  ref.apply(ref.SRGB, linear))))
            corners = [(max((q for q in range(101) if tones[q] <= c), default=0),
                        min((q for q in range(101) if tones[q] >= c), default=100))
                       for c in restored]
            weights = list(map(D, ['.85', '.075', '.075'])) if space > 1 else [D(1)]*3
            def distance(q):
                target = typed([tones[v] for v in q], space)
                return sum(w*(a-b)**2 for w,a,b in zip(weights,target,source))
            candidates = sorted(set(product(*corners)), key=distance)
            if len(candidates) > 1:
                assert distance(candidates[1])-distance(candidates[0]) > D('1e-9')
            nearest = candidates[0]
            byte = [int(max(D(0),min(D(1),c))*255+D('.5')) for c in restored]
            reversible = [(100*v+127)//255 for v in byte]
            rows.append([nearest, reversible])
            wire_fixture.append([[space_ref.wire([
                space_ref.f32((1-rate)*a+rate*b) for a,b in zip(source,
                    typed([tones[q] for q in target], space))], name, True)[0]
                for rate in [D(1),D('.25')]] for target in [nearest,reversible]])
            out.append('        { /* '+name+' */')
            for row in [source, typed([tones[q] for q in nearest],space),
                        typed([tones[q] for q in reversible],space)]:
                out.append('            {')
                out.extend('                '+space_ref.literal(v)+',' for v in row)
                out.append('            },')
            out.append('        },')
        codes.append(rows)
        wire_rows.append(wire_fixture)
        out.append('    },')
    out += ['};', 'static unsigned int const p3_snap_codes[3][5][2] = {']
    for fixture in codes:
        out.append('    {')
        for row in fixture:
            out.append('        { '+', '.join(str(q[0]*10201+q[1]*101+q[2]) for q in row)+' },')
        out.append('    },')
    out += ['};', '/* fixture, space, policy, full/quarter rate, RGB101. */',
            'static unsigned char const p3_snap_wire[3][5][2][2][3] = {']
    for fixture in wire_rows:
        out.append('    {')
        for row in fixture:
            out.append('        {')
            for policy in row:
                out.append('            { '+', '.join('{ '+', '.join(map(str,v))+' }' for v in policy)+' },')
            out.append('        },')
        out.append('    },')
    out += ['};', '/* Hard sRGB corners snapped in gamma, stored in linear. */',
            'static double const p3_snap_cover[2][8][3] = {']
    for rate in [D(1), D('.25')]:
        out.append('    {')
        for source in product([D(0), D(1)], repeat=3):
            destination = list(map(ref.encode, ref.apply(ref.inverse(ref.P3),
                ref.apply(ref.SRGB, source))))
            corners = [(max((q for q in range(101) if tones[q] <= c), default=0),
                        min((q for q in range(101) if tones[q] >= c), default=100))
                       for c in destination]
            candidates = sorted(set(product(*corners)), key=lambda q:
                sum((a-b)**2 for a,b in zip(source,
                    typed([tones[v] for v in q], 0))))
            target = typed([tones[q] for q in candidates[0]], 0)
            blended = [space_ref.f32((1-rate)*a+rate*b)
                       for a,b in zip(source, target)]
            out.append('        {')
            out.extend('            '+space_ref.literal(ref.decode(v))+','
                       for v in blended)
            out.append('        },')
        out.append('    },')
    return '\n'.join(out+['};','#endif','#endif',''])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1]/'tests/data/color-numeric/p3-snap.h'
    text = build()
    if args.check:
        if path.read_text() != text:
            parser.exit(1, 'stale P3 snap fixtures\n')
    else:
        path.write_text(text)


if __name__ == '__main__':
    main()
