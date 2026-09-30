#!/usr/bin/env python3
"""Generate typed snap expectations without importing or executing libsixel."""
import argparse
from decimal import Decimal as D
from itertools import product
from pathlib import Path
import generate_color_fixtures as ref
import generate_palette_space_fixtures as space_ref


def typed(rgb, space):
    linear = list(map(ref.decode, rgb))
    values = [rgb, linear, *ref.opponents(linear)][space]
    return [D(str(v)) for v in values]


def wire(values, name):
    codes, margin = space_ref.wire(values, name, False)
    assert D(margin) > D('.02')
    return codes


def legacy_wire(values, name, output):
    linear = space_ref.linear_from(values, name)
    if output == 0:
        converted = linear
    else:
        # Fixed legacy SMPTE-C matrix is the existing option contract, not a
        # newly calibrated profile. Evaluate its equations in Decimal;
        # this fixture does not read or call the production converter.
        matrix = ref.matrix([
            ['1.0651944799343782', '-.05539144537002962', '-.009975616485882548'],
            ['-.019633066659433226', '1.0363870284433383', '-.016731961783904975'],
            ['.0016324889176928742', '.004413466273704836', '.994192644808602']])
        converted = [abs(v)**(1/D('2.2')) * (-1 if v < 0 else 1)
                     for v in ref.apply(matrix, linear)]
    scaled = [max(D(0), min(D(1), v))*100 for v in converted]
    assert min(abs(v-int(v)-D('.5')) for v in scaled) > D('.02')
    return [int(v+D('.5')) for v in scaled]


def build():
    tones = [D((255*q+50)//100)/255 for q in range(101)]
    sources = [[D('-.25'), D('1.25'), D('98.125')/255],
               [D('.351'), D('.533'), D('.712')]]
    # Existing CLI witness: linear nearest chooses blue 31, not source 32.
    # Independently protect its comment/expected code when regenerating.
    source = list(map(space_ref.f32, typed([D(v)/255 for v in [128,64,32]], 1)))
    candidates = product([D(128)/255], [D(64)/255], [D(31)/255,D(33)/255])
    best = min(candidates, key=lambda rgb: sum((a-b)**2
               for a,b in zip(source, typed(rgb, 1))))
    assert best[2]*255 == 31
    assert legacy_wire(list(map(space_ref.f32, typed(best, 1))), 'linear', 1) == [51,25,14]
    wire_rows = []
    output_rows = []
    out = ['/* Independent oracle: generate_snap_native_fixtures.py. */',
           '#ifndef COLOR_SNAP_NATIVE_H', '#define COLOR_SNAP_NATIVE_H',
           '/* source, nearest, reversible; each in the named typed space. */',
           'static double const snap_native_fixture[2][5][3][3] = {']
    for rgb in sources:
        out.append('    {')
        fixture_wires = []
        for space, name in enumerate(space_ref.NAMES):
            source = list(map(space_ref.f32, typed(rgb, space)))
            restored = list(map(ref.encode, space_ref.linear_from(source, name)))
            corners = [(max((v for v in tones if v <= c), default=D(0)),
                        min((v for v in tones if v >= c), default=D(1)))
                       for c in restored]
            weights = [D('.85'), D('.075'), D('.075')] if space > 1 else [D(1)]*3
            def distance(candidate):
                target = typed(candidate, space)
                return sum(w*(a-b)**2 for w,a,b in zip(weights, target, source))
            candidates = sorted(set(product(*corners)), key=distance)
            if len(candidates) > 1:
                # Source/transform rounding must not decide which corner wins.
                assert distance(candidates[1])-distance(candidates[0]) > D('1e-9')
            nearest = typed(candidates[0], space)
            byte = [int(max(D(0), min(D(1), c))*255+D('.5')) for c in restored]
            # Legacy reversible first rounds to a byte, then rounds its
            # percentage, then reconstructs the decoder's safe byte.
            reversible = typed([D((255*((100*v+127)//255)+50)//100)/255
                                for v in byte], space)
            out.append('        { /* '+name+' */')
            for row in [source, nearest, reversible]:
                out.append('            {')
                out.extend('                '+space_ref.literal(v)+',' for v in row)
                out.append('            },')
            out.append('        },')
            if rgb is sources[0]:
                output_rows.append([[[legacy_wire(
                    [space_ref.f32((1-rate)*a+rate*b)
                     for a,b in zip(source,target)], name, output)
                    for rate in [D(1),D('.25')]]
                    for target in [nearest,reversible]] for output in [0,1]])
            fixture_wires.append([[wire([space_ref.f32((1-rate)*a+rate*b)
                for a,b in zip(source,target)], name)
                for rate in [D(1),D('.25')]]
                for target in [nearest,reversible]])
        out.append('    },')
        wire_rows.append(fixture_wires)
    out += ['};', 'static unsigned char const snap_native_wire[2][5][2][2][3] = {']
    for fixture in wire_rows:
        out.append('    {')
        for row in fixture:
            out.append('        {')
            for policy in row:
                out.append('            { '+', '.join('{ '+', '.join(map(str,v))+' }' for v in policy)+' },')
            out.append('        },')
        out.append('    },')
    out += ['};', '/* space, output linear/SMPTE-C, policy, rate, RGB101. */',
            'static unsigned char const snap_native_output[5][2][2][2][3] = {']
    for space in output_rows:
        out.append('    {')
        for output in space:
            out.append('        {')
            for policy in output:
                out.append('            { '+', '.join('{ '+', '.join(map(str,v))+' }' for v in policy)+' },')
            out.append('        },')
        out.append('    },')
    return '\n'.join(out+['};', '#endif', ''])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    path = Path(__file__).resolve().parents[1]/'tests/data/color-numeric/snap-native.h'
    text = build()
    if a.check:
        if path.read_text() != text:
            p.exit(1, 'stale native snap fixtures\n')
    else:
        path.write_text(text)


if __name__ == '__main__':
    main()
