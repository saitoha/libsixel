#!/usr/bin/env python3
"""Derive CLI wire expectations from original samples and independent CMS data."""

import argparse
from decimal import Decimal as D, localcontext
import hashlib
import json
from pathlib import Path
import struct
import zlib

import tiff_managed_reference as reference


INPUTS = ('png-samples.json', 'png-p3-16-rgba.png', 'tiff-layout.json',
          'tiff-layout-rgb16-o6.tif', 'display-p3.icc', 'tiff-ycbcr.json',
          'png-p3-8-rgb.png', 'png-p3-16-rgb.png', 'png-raw-8-rgb.png')


def embedded_profile(blob):
    if blob.startswith(b'\x89PNG'):
        offset = 8
        while offset < len(blob):
            size = struct.unpack_from('>I', blob, offset)[0]
            if blob[offset + 4:offset + 8] == b'iCCP':
                data = blob[offset + 8:offset + 8 + size]
                name_end = data.index(0)
                assert data[name_end + 1] == 0
                return zlib.decompress(data[name_end + 2:])
            offset += size + 12
    else:
        assert blob[:4] == b'II*\0'
        offset = struct.unpack_from('<I', blob, 4)[0]
        count = struct.unpack_from('<H', blob, offset)[0]
        for index in range(count):
            tag, kind, size, start = struct.unpack_from(
                '<HHII', blob, offset + 2 + 12 * index)
            if tag == 34675:
                assert kind == 7
                return blob[start:start + size]
    raise ValueError('missing original ICC profile')


def wire_color(linear):
    result = []
    for component in linear:
        value = min(D(1), max(D(0), D(str(component))))
        gamma = (D('12.92') * value if value <= D('.0031308') else
                 D('1.055') * value ** (D(1) / D('2.4')) - D('.055'))
        percent = gamma * 100
        rounded = int(percent + D('.5'))
        # This is a fixture-selection margin, not an implementation tolerance.
        # Stay away from wire half steps; runtime assertions remain exact.
        assert abs(percent - (D(rounded) - D('.5'))) > D('.01')
        assert abs(percent - (D(rounded) + D('.5'))) > D('.01')
        result.append(rounded)
    return result


def p3_matrix():
    """Derive the ideal P3/D65 basis independently of CMS implementation."""
    columns = [reference.xyz(reference.F(x), reference.F(y))
               for x, y in [('.68', '.32'), ('.265', '.69'), ('.15', '.06')]]
    unscaled = list(map(list, zip(*columns)))
    scale = reference.mv(reference.inv(unscaled), reference.white)
    return [[unscaled[i][j] * scale[j] for j in range(3)] for i in range(3)]


def p3_wire_color(linear):
    conversion = reference.matmul(reference.inv(p3_matrix()), reference.native)
    output = reference.mv(conversion, list(map(reference.F, linear)))
    # P3 and sRGB use the same transfer function. The matrix must precede
    # this final gamut clamp; clamping canonical RGB destroys P3 primaries.
    return wire_color([D(x.numerator) / D(x.denominator) for x in output])


def build(inputs):
    png = json.loads(inputs['png-samples.json'])
    png_source = next(x for x in png['pngs']
                      if x['file'] == 'png-p3-16-rgba.png')
    tiff = next(x for x in json.loads(inputs['tiff-layout.json'])['fixtures']
                if x['name'] == 'rgb16-o6')
    for item in (png_source, tiff):
        blob = inputs[item['file']]
        assert hashlib.sha256(blob).hexdigest() == item['sha256']
        assert embedded_profile(blob) == inputs['display-p3.icc']

    # Invert orientation 6 independently of the TIFF fixture writer's scatter.
    # The displayed 2x3 image reads original (x=dy, y=H-1-dx).
    assert (tiff['width'], tiff['height'], tiff['orientation']) == (3, 2, 6)
    triples = []
    for y in range(3):
        for x in range(2):
            sample = tiff['samples'][(1 - x) * 3 + y]
            triples.append([struct.unpack('>f', struct.pack(
                '>I', reference.nearest_rational(reference.F(v, 65535))))[0]
                            for v in sample])
    linear = reference.expected({'icc': inputs['display-p3.icc'],
                                 'triples': triples})
    cases = []
    with localcontext() as context:
        context.prec = 60
        composed = []
        for sample, rgb in zip(png_source['samples'],
                               png['expected']['icc_linear'][1]):
            alpha = D(sample[3]) / 65535
            # keep retains hidden RGB at exact alpha zero. Fractional alpha
            # is resolved in linear light against the explicit black backdrop.
            composed.append([D(str(v)) * (alpha if alpha else 1) for v in rgb])
        cases.append(dict(name='png-rgba16', source=png_source['file'],
                          width=8, height=4, source_width=4, scale=2,
                          hidden=[int(p[3] == 0) for p in png_source['samples']],
                          linear=[list(map(str, p)) for p in composed], p2=1,
                          wire=[wire_color(p) for p in composed]))
        triples = [linear[i:i + 3] for i in range(0, len(linear), 3)]
        cases.append(dict(name='tiff-rgb16-o6', source=tiff['file'],
                          width=8, height=12, source_width=2, scale=4,
                          hidden=[0] * 6, linear=[list(map(str, p)) for p in triples],
                          p2=0, wire=[wire_color(p) for p in triples]))

    ycbcr = json.loads(inputs['tiff-ycbcr.json'])
    triples = [ycbcr['linear'][i:i + 3] for i in range(0, 18, 3)]
    cases.append(dict(name='tiff-ycbcr-rgb8',
                      source='tiff-legacy-ycbcr8-111.tif', width=6, height=4,
                      source_width=3, scale=2, hidden=[0] * 6, p2=0,
                      linear=[list(map(str, p)) for p in triples],
                      wire=ycbcr['wire']))

    with localcontext() as context:
        context.prec = 60
        for name in ('png-p3-8-rgb.png', 'png-p3-16-rgb.png',
                     'png-raw-8-rgb.png'):
            item = next(x for x in png['pngs'] if x['file'] == name)
            assert hashlib.sha256(inputs[name]).hexdigest() == item['sha256']
            maximum = 65535 if '16' in name else 255
            samples = [[struct.unpack('>f', struct.pack('>I',
                        reference.nearest_rational(reference.F(x, maximum))))[0]
                        for x in rgb] for rgb in item['samples']]
            if 'raw' in name:
                triples = [[D(str(x)) / D('12.92') if D(str(x)) <= D('.04045')
                            else ((D(str(x)) + D('.055')) / D('1.055')) ** D('2.4')
                            for x in rgb] for rgb in samples]
            else:
                assert embedded_profile(inputs[name]) == inputs['display-p3.icc']
                flat = reference.expected(dict(icc=inputs['display-p3.icc'],
                                                triples=samples))
                triples = [flat[i:i + 3] for i in range(0, len(flat), 3)]
            cases.append(dict(name=name[:-4] + '-output-p3', source=name,
                              width=4, height=2, source_width=4, scale=1,
                              hidden=[0] * 8, p2=0,
                              linear=[list(map(str, rgb)) for rgb in triples],
                              wire=[p3_wire_color(rgb) for rgb in triples]))
        triples = cases[1]['linear']
        cases.append(dict(name='tiff-rgb16-o6-output-p3', source=tiff['file'],
                          width=2, height=3, source_width=2, scale=1,
                          hidden=[0] * 6, p2=0, linear=triples,
                          wire=[p3_wire_color(rgb) for rgb in triples]))


    with localcontext() as context:
        context.prec = 60
        # This selected seed has two stable linear clusters: the first red
        # sample and the other seven samples. Derive both centers from those
        # source groups and independently verify their nearest-center cells.
        source = cases[4]
        points = [[D(x) for x in rgb] for rgb in source['linear']]
        centers = [points[0], [sum(rgb[c] for rgb in points[1:]) / D(7)
                               for c in range(3)]]
        cluster_margins = []
        for index, point in enumerate(points):
            distances = [sum((point[c] - center[c]) ** 2 for c in range(3))
                         for center in centers]
            owner = 0 if index == 0 else 1
            assert distances[owner] < distances[1 - owner]
            cluster_margins.append(str(abs(distances[0] - distances[1])))
        palette = [p3_wire_color(rgb) for rgb in centers]

        def encode_extended(value):
            absolute = abs(value)
            result = (D('12.92') * absolute if absolute <= D('.0031308') else
                      D('1.055') * absolute ** (D(1) / D('2.4')) - D('.055'))
            return result if value >= 0 else -result

        # Snap is disabled: lookup and diffusion use the unrounded centers.
        # Integer wire colors are a separate serialization-only projection.
        working_palette = [[encode_extended(x) for x in rgb] for rgb in centers]
        working = [[encode_extended(x) for x in rgb] for rgb in points]
        painted = []
        fs_margins = []
        # Independent raster FS, exact rational weights and Decimal error.
        for y in range(2):
            for x in range(4):
                index = y * 4 + x
                distances = [sum((working[index][c] - color[c]) ** 2
                                 for c in range(3)) for color in working_palette]
                owner = 0 if distances[0] < distances[1] else 1
                fs_margins.append(str(abs(distances[0] - distances[1])))
                assert abs(distances[0] - distances[1]) > D('.001')
                painted.append(palette[owner])
                error = [working[index][c] - working_palette[owner][c] for c in range(3)]
                for dx, dy, numerator in ((1, 0, 7), (-1, 1, 3),
                                          (0, 1, 5), (1, 1, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < 4 and ny < 2:
                        for c in range(3):
                            working[ny * 4 + nx][c] += error[c] * D(numerator) / 16
        cases.append(dict(name='png-p3-16-two-cluster-fs-output-p3',
                          source=source['source'], width=4, height=2,
                          source_width=4, scale=1, hidden=[0] * 8, p2=0,
                          cluster_members=[[0], list(range(1, 8))],
                          centers_linear=[list(map(str, rgb)) for rgb in centers],
                          cluster_distance_margins=cluster_margins,
                          fs_lookup_distance_margins=fs_margins,
                          linear=source['linear'], wire=painted))

    header = ('/* Generated independently; do not copy encoder output here. */\n'
              'static struct {\n'
              '    int width, height, source_width, scale, count, p2;\n'
              '    unsigned char hidden[8];\n'
              '    unsigned char rgb[8][3];\n'
              '} const color_cli_wire[] = {\n')
    for case in cases:
        fields = [case[k] for k in ('width', 'height', 'source_width', 'scale')]
        fields += [len(case['wire']), case['p2']]
        header += '    { ' + ', '.join(map(str, fields)) + ',\n'
        header += '      { ' + ', '.join(map(str, case['hidden'])) + ' }, {\n'
        header += ''.join('        { ' + ', '.join(map(str, rgb)) + ' },\n'
                          for rgb in case['wire'])
        header += '    } },\n'
    header += '};\n'
    record = dict(generator='tools/generate_cli_color_fixtures.py',
                  method='Existing independent PNG frame values; original TIFF '
                         'UINT16/F32 and stored-ICC rational/Decimal oracle; '
                         'linear black composition, nearest sample replication, '
                         'sRGB transfer and nearest integer percent; selected '
                         'P3 cases use the independently derived ideal P3/D65 '
                         'matrix before the final output gamut clamp.',
                  scope='Selected CLI wire values, not a perceptual guarantee '
                        'or complete coverage of intermediate precision.',
                  input_sha256={n: hashlib.sha256(inputs[n]).hexdigest()
                                for n in INPUTS}, cases=cases)
    with localcontext() as context:
        context.prec = 60
        inverse = reference.matmul(reference.inv(reference.native), p3_matrix())
        red = reference.mv(inverse, [reference.F(1), reference.F(0), reference.F(0)])
        palette_header = '/* Independently derived ideal P3 output coordinates. */\n'
        palette_header += 'static double const color_p3_red_linear[3] = {\n'
        palette_header += ''.join('    ' + str(D(x.numerator) / D(x.denominator)) + ',\n'
                                  for x in red)
        palette_header += '};\nstatic double const color_p3_gray_linear[2] = {\n'
        palette_header += ''.join('    ' + str(((D(x) + D('.055')) / D('1.055'))
                                              ** D('2.4')) + ',\n'
                                  for x in ('.35', '.37'))
        palette_header += '};\n'
    return {'cli-color-wire.h': header.encode(),
            'cli-color-wire.json': (json.dumps(record, indent=2) + '\n').encode(),
            'p3-palette.h': palette_header.encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / 'tests/data/color-numeric'
    for name, content in build({n: (root / n).read_bytes() for n in INPUTS}).items():
        if name.endswith('.h'):
            guard = 'COLOR_FIXTURE_' + name[:-2].upper().replace('-', '_')
            content = (f'#ifndef {guard}\n#define {guard}\n'.encode() +
                       content + b'#endif\n')
        if args.check:
            if (root / name).read_bytes() != content:
                parser.exit(1, f'stale CLI color expectation: {name}\n')
        else:
            (root / name).write_bytes(content)
    print('CLI color expectations verified' if args.check else
          'CLI color expectations generated')


if __name__ == '__main__':
    main()
