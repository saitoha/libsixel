#!/usr/bin/env python3
"""Independent destination snap oracle, without loading production code."""
import argparse
from decimal import Decimal as D
from itertools import product
from pathlib import Path
import generate_color_fixtures as ref
import generate_palette_space_fixtures as spaces

# Adobe RGB (1998), DCI-P3 and an unnamed, unequal-channel analytic model.
DEFINITIONS = [
    ('.3127 .3290 .64 .33 .21 .71 .15 .06', [D(563)/256]*3),
    ('.3140 .3510 .68 .32 .265 .69 .15 .06', [D('2.6')]*3),
    ('.3127 .3290 .68 .32 .265 .69 .15 .06', list(map(D, ['1.8','2.2','2.6']))),
]


def matrix(coords):
    xy = coords.split()
    white = ref.white(*xy[:2])
    columns = [ref.white(*xy[i:i+2]) for i in (2, 4, 6)]
    raw = list(map(list, zip(*columns)))
    scales = ref.apply(ref.inverse(raw), white)
    raw = [[v*scales[j] for j, v in enumerate(row)] for row in raw]
    src = ref.apply(ref.BRADFORD, white)
    dst = ref.apply(ref.BRADFORD, ref.D50)
    def adapt(v):
        cone = ref.apply(ref.BRADFORD, v)
        return ref.apply(ref.inverse(ref.BRADFORD),
                         [c*d/s for c,d,s in zip(cone,dst,src)])
    return list(map(list, zip(*(adapt(col) for col in zip(*raw)))))


def power(value, exponent):
    return abs(value)**exponent * (-1 if value < 0 else 1)


def build():
    srgb = matrix('.3127 .3290 .64 .33 .30 .60 .15 .06')
    tones = [D((q*255+50)//100)/255 for q in range(101)]
    values, codes, wires = [], [], []
    for coords, gamma in DEFINITIONS:
        m = matrix(coords)
        def typed(encoded, space):
            linear = ref.apply(ref.inverse(srgb), ref.apply(m,
                [power(v,g) for v,g in zip(encoded,gamma)]))
            return [D(str(v)) for v in [list(map(ref.encode,linear)), linear,
                    *ref.opponents(linear)][space]]
        def encoded(source, space):
            linear = spaces.linear_from(source, spaces.NAMES[space])
            device = ref.apply(ref.inverse(m), ref.apply(srgb, linear))
            return [power(v,1/g) for v,g in zip(device,gamma)]
        all_values, all_codes, all_wires = [], [], []
        for rgb in [['.973','.053','.157'], ['.351','.533','.712'], ['-.103','1.057','.403']]:
            fixture_values, fixture_codes, fixture_wires = [], [], []
            for space in range(5):
                source = list(map(spaces.f32, typed(list(map(D,rgb)),space)))
                device = encoded(source,space)
                # A cell boundary changes the candidate set. Exclude it as
                # well as distance/rounding ties so FPU inversion error cannot
                # choose the adjacent cell, especially for clipped sources.
                assert all(v<=0 or v>=1 or min(abs(v-t) for t in tones)>D('2e-5')
                           for v in device)
                corners = [(max((q for q in range(101) if tones[q]<=v),default=0),
                            min((q for q in range(101) if tones[q]>=v),default=100))
                           for v in device]
                weights = list(map(D,['.85','.075','.075'])) if space>1 else [D(1)]*3
                def distance(q):
                    target = typed([tones[c] for c in q],space)
                    return sum(w*(a-b)**2 for w,a,b in zip(weights,target,source))
                candidates = sorted(set(product(*corners)), key=distance)
                if len(candidates)>1:
                    assert distance(candidates[1])-distance(candidates[0])>D('1e-9')
                byte = [int(max(D(0),min(D(1),v))*255+D('.5')) for v in device]
                targets = [candidates[0], [(100*v+127)//255 for v in byte]]
                rows = [typed([tones[q] for q in target],space) for target in targets]
                fixture_values.append([source,*rows])
                fixture_codes.append([q[0]*10201+q[1]*101+q[2] for q in targets])
                wire_policies = []
                for row in rows:
                    wire_rates = []
                    for rate in [D(1),D('.25')]:
                        blended = [spaces.f32((1-rate)*a+rate*b) for a,b in zip(source,row)]
                        percents = [max(D(0),min(D(1),v))*100 for v in encoded(blended,space)]
                        assert min(abs(v-(int(v)+D('.5'))) for v in percents)>D('.002')
                        wire_rates.append([int(v+D('.5')) for v in percents])
                    wire_policies.append(wire_rates)
                fixture_wires.append(wire_policies)
            all_values.append(fixture_values); all_codes.append(fixture_codes); all_wires.append(fixture_wires)
        values.append(all_values); codes.append(all_codes); wires.append(all_wires)
    def array(v, indent=0):
        if not isinstance(v,list):
            return str(v) if isinstance(v,int) else spaces.literal(v)
        parts = [array(x,indent+4) for x in v]
        compact = '{ '+', '.join(parts)+' }'
        if '\n' not in compact and indent+len(compact)<=78:
            return compact
        return '{\n'+' '*(indent+4)+(',\n'+' '*(indent+4)).join(parts)+'\n'+' '*indent+'}'
    text = ['/* Independent oracle: generate_destination_snap_fixtures.py. */',
            '#ifndef COLOR_DESTINATION_SNAP_H', '#define COLOR_DESTINATION_SNAP_H']
    for typ,name,shape,data in [('double','values','[3][3][5][3][3]',values),
        ('unsigned int','codes','[3][3][5][2]',codes),
        ('unsigned char','wire','[3][3][5][2][2][3]',wires)]:
        text.append(f'static {typ} const destination_snap_{name}{shape} = '+array(data)+';')
    return '\n'.join(text+['#endif',''])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    path=Path(__file__).resolve().parents[1]/'tests/data/color-numeric/destination-snap.h'
    text=build()
    if args.check:
        if path.read_text()!=text:
            parser.exit(1,'stale destination snap fixtures\n')
    else:
        path.write_text(text)


if __name__=='__main__':
    main()
