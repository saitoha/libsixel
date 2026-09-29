#!/usr/bin/env python3
"""Independent extended palette centroids and RGB101 boundary expectations."""
import argparse
from decimal import Decimal as D
import json
from pathlib import Path
import struct
import generate_color_fixtures as ref

NAMES = ['gamma', 'linear', 'oklab', 'cielab', 'din99d']
LMS_INV = ref.inverse(ref.matrix([
    ['.4122214708', '.5363325363', '.0514459929'],
    ['.2119034982', '.6806995451', '.1073969566'],
    ['.0883024619', '.2817188376', '.6299787005']]))
OK_INV = ref.inverse(ref.matrix([
    ['.2104542553', '.7936177850', '-.0040720468'],
    ['1.9779984951', '-2.4285922050', '.4505937099'],
    ['.0259040371', '.7827717662', '-.8086757660']]))
XYZ_INV = ref.inverse(ref.matrix([
    ['.4124564', '.3575761', '.1804375'],
    ['.2126729', '.7151522', '.0721750'],
    ['.0193339', '.1191920', '.9503041']]))
ANGLE = D('3.141592653589793238462643383279502884197169399375105820974944') * 50 / 180
SIN = term = ANGLE
COS = cterm = D(1)
for n in range(1, 60):
    term *= -ANGLE * ANGLE / ((2*n) * (2*n+1))
    cterm *= -ANGLE * ANGLE / ((2*n-1) * (2*n))
    SIN += term
    COS += cterm


def f32(value):
    return D.from_float(struct.unpack('>f', struct.pack('>f', float(value)))[0])


def linear_from(value, space):
    if space == 'gamma':
        return list(map(ref.decode, value))
    if space == 'linear':
        return value
    if space == 'oklab':
        return ref.apply(LMS_INV, [v**3 for v in ref.apply(OK_INV, value)])
    light, a, b = value
    din = space == 'din99d'
    if din:
        light, a, b = light*100, a*100, b*100
        light = ((light/D('325.22')).exp()-1)/D('.0036')
        chroma = (a*a+b*b).sqrt()
        gain = (((chroma/D('22.5')).exp()-1)/D('.06')/chroma) if chroma else D(0)
        e = gain*(COS*a+SIN*b)
        f = gain*(-SIN*a+COS*b)/D('1.14')
        a, b = COS*e-SIN*f, SIN*e+COS*f
    else:
        light, a, b = light*100, a*128, b*128
    fy = (light+16)/116
    roots = [fy+a/500, fy, fy-b/200]
    xyz = [v**3 if v > D(6)/29 else (116*v-16)*27/24389 for v in roots]
    xn = D('1.12')*D('.95047')-D('.12')*D('1.08883') if din else D('.95047')
    xyz[0] *= xn
    xyz[2] *= D('1.08883')
    if din:
        xyz[0] = (xyz[0]+D('.12')*xyz[2])/D('1.12')
    return ref.apply(XYZ_INV, xyz)


def wire(value, space, p3):
    linear = linear_from(value, space)
    if p3:
        linear = ref.apply(ref.inverse(ref.P3), ref.apply(ref.SRGB, linear))
    percent = [max(D(0), min(D(1), ref.encode(v)))*100 for v in linear]
    # Cases must be far from a rounding tie under float32 storage and the
    # legacy working-space inverse coefficients. Do not tighten to bitwise
    # arithmetic equality or derive values from a production round trip.
    margin = min(abs(v-(int(v)+D('.5'))) for v in percent)
    assert margin > D('.002'), (space, p3, percent, margin)
    return [int(v+D('.5')) for v in percent], str(margin)


def literal(value):
    if isinstance(value, int):
        return str(value)
    text = format(float(value), '.17g')
    return text if '.' in text or 'e' in text else text+'.0'


def build():
    source = [list(map(f32, map(D, row))) for row in
              [('2', '.1', '.1'), ('-.25', '.1', '.1')]]
    records = []
    for space in NAMES:
        samples = []
        for rgb in source:
            values = (list(map(ref.encode, rgb)) if space == 'gamma' else rgb
                      if space == 'linear' else ref.opponents(rgb)[NAMES.index(space)-2])
            samples.append(list(map(f32, values)))
        center = [f32((samples[0][n]+samples[1][n])/2) for n in range(3)]
        entries = [list(map(f32, map(D, row))) for row in
                   [('.423', '.122', '-.073'), ('.442', '.147', '-.09')]]
        residual = list(map(D, ['.312', '.123', '-.127']))
        records.append(dict(space=space, samples=samples, center=center,
            wire=[wire(center, space, p)[0] for p in [False, True]],
            margins=[wire(center, space, p)[1] for p in [False, True]],
            lookup=entries, lookup_wire=[[wire(c, space, p)[0] for c in entries]
                                         for p in [False, True]],
            residual_wire=[wire(residual, space, p)[0] for p in [False, True]]))
    def array(value, indent=8):
        if isinstance(value, list):
            parts = [array(v, indent+4) for v in value]
            inline = '{ '+', '.join(parts)+' }'
            if '\n' not in inline and indent+len(inline) < 78:
                return inline
            return '{\n'+' '*(indent+4)+(',\n'+' '*(indent+4)).join(parts)+'\n'+' '*indent+'}'
        return literal(value)
    header = ('/* Generated independently by tools/generate_palette_space_fixtures.py. */\n'
        '#ifndef COLOR_PALETTE_SPACE_H\n#define COLOR_PALETTE_SPACE_H\n'
        'static struct {\n    double samples[2][3];\n    double center[3];\n'
        '    unsigned char wire[2][3];\n    double lookup[2][3];\n'
        '    unsigned char lookup_wire[2][2][3];\n'
        '    unsigned char residual_wire[2][3];\n'
        '} const color_palette_space[5] = {\n')
    for r in records:
        header += '    { /* '+r['space']+' */\n'
        for key in ['samples', 'center', 'wire', 'lookup', 'lookup_wire', 'residual_wire']:
            header += '        '+array(r[key])+',\n'
        header += '    },\n'
    header += '};\n#endif\n'
    record = dict(generator='tools/generate_palette_space_fixtures.py',
        arithmetic='Independent forward equations, explicit binary32 storage, '
                   '60-digit Decimal inverse of published forward matrices; '
                   'canonical legacy D65 Lab/DIN99d conventions; '
                   'chromaticity-derived sRGB/P3 conversion; final RGB101 rounding.',
        source_linear=source, cases=records)
    return {'palette-space.h':header.encode(), 'palette-space.json':
            (json.dumps(record, default=str, indent=2)+'\n').encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]/'tests/data/color-numeric'
    for name, data in build().items():
        if args.check:
            if (root/name).read_bytes() != data:
                parser.exit(1, 'stale palette space fixture: '+name+'\n')
        else:
            (root/name).write_bytes(data)
    print('palette space fixtures verified' if args.check else 'palette space fixtures generated')


if __name__ == '__main__':
    main()
