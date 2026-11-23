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
from generate_builtin_native_fixtures import XTERM16


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


def cli_space_cases():
    def coordinates(rgb, linear):
        encoded = [D(component) / 255 for component in rgb]
        if not linear:
            return encoded
        return [x / D('12.92') if x <= D('.04045') else
                ((x + D('.055')) / D('1.055')) ** D('2.4') for x in encoded]

    def nearest(point, palette):
        distances = sorted((sum((x - y)**2 for x, y in zip(point, color)), i)
                           for i, color in enumerate(palette))
        margin = distances[1][0] - distances[0][0]
        assert margin > D('.009')
        return distances[0][1], str(margin)

    def case(name, samples, **data):
        width = len(samples)
        return dict(name=name, source='inline PNM', samples=samples, width=width,
                    height=1, source_width=width, scale=1, hidden=[0] * width,
                    p2=0, **data)

    # Two nearby warm samples and a separate cyan sample. Both coordinate
    # systems give the same partition, so the changed RGB101 value is due to
    # the centroid calculation itself.
    samples = [[223, 31, 25], [72, 57, 29], [16, 169, 221]]
    for space in ('gamma', 'linear'):
        linear = space == 'linear'
        points = [coordinates(rgb, linear) for rgb in samples]
        centers = [[(x + y) / 2 for x, y in zip(points[0], points[1])],
                   points[2]]
        owners, margins = zip(*(nearest(point, centers) for point in points))
        assert owners == (0, 0, 1)
        if linear:
            wire = [wire_color(center) for center in centers]
        else:
            assert all(abs(100*x - int(100*x) - D('.5')) > D('.01')
                       for center in centers for x in center)
            wire = [[int(100*x + D('.5')) for x in center] for center in centers]
        yield case('pnm-cluster-' + space, samples, cluster_distance_margins=margins,
                   wire=[wire[i] for i in owners])

    # The same first color wins in both spaces. Its residual is encoded in
    # different coordinates. In a one-row raster, only the 7/16 right tap
    # reaches the second pixel, and it changes the selected entry in both.
    samples = [[47, 195, 161], [70, 56, 192]]
    srgb_wire = [wire_color(coordinates(rgb, True)) for rgb in XTERM16]
    for space in ('gamma', 'linear'):
        linear = space == 'linear'
        points = [coordinates(rgb, linear) for rgb in samples]
        palette = [coordinates(rgb, linear) for rgb in XTERM16]
        first, first_margin = nearest(points[0], palette)
        uncorrected, uncorrected_margin = nearest(points[1], palette)
        candidate = [x + D(7)/16*(y - p)
                     for x, y, p in zip(points[1], points[0], palette[first])]
        assert all(0 < x < 1 for x in candidate)
        corrected, corrected_margin = nearest(candidate, palette)
        assert corrected != uncorrected
        yield case('pnm-lookup-' + space, samples, fixed_palette='xterm16',
                   lookup_distance_margins=[first_margin, uncorrected_margin],
                   wire=[srgb_wire[first], srgb_wire[uncorrected]])
        yield case('pnm-fs-' + space, samples, fixed_palette='xterm16',
                   lookup_distance_margins=[first_margin, corrected_margin],
                   second_candidate=list(map(str, candidate)),
                   wire=[srgb_wire[first], srgb_wire[corrected]])


def mixed_spaces_cases():
    from generate_color_fixtures import apply, encode, matrix, opponents
    from generate_tiff_logluv_tests import tiff, words_and_centers

    def decimal(fraction):
        return D(fraction.numerator) / D(fraction.denominator)

    def decode(value, maximum):
        return ((D(value) / maximum + D('.055')) / D('1.055')) ** D('2.4')

    def centroid(points):
        return [sum(channel) / len(channel) for channel in zip(*points)]

    def distance(a, b):
        return sum((x-y)**2 for x, y in zip(a, b))

    def diffuse(points, palette, bounded=False):
        error = [D(0)] * 3
        owners = []
        for row in points:
            query = [x + e * D(7) / 16 for x, e in zip(row, error)]
            if bounded:
                assert all(0 < x < 1 for x in query)
            a, b = (distance(query, color) for color in palette)
            assert abs(a - b) > D('.01')
            owner = int(a > b)
            owners.append(owner)
            error = [x - y for x, y in zip(query, palette[owner])]
        return owners

    native = [[decimal(x) for x in row] for row in reference.native]
    inverse = [[decimal(x) for x in row]
               for row in reference.inv(reference.native)]
    levels = [decode(code, 255) for code in (34, 120, 105, 146, 183)]
    words, xyz = words_and_centers([apply(native, [y]*3) for y in levels], 32)
    xyz_as_rgb = [apply(inverse, row) for row in xyz]
    rgb = [[int(D(65535) * D(str(encode(x))) + D('.5')) for x in row]
           for row in xyz_as_rgb]
    fixtures = {
        'mixed-spaces-xyz.tif': tiff(words, 32, width=5, height=1),
        'mixed-spaces-rgb.ppm': b'P6\n5 1\n65535\n' +
            b''.join(struct.pack('>3H', *row) for row in rgb),
    }
    lms_root = matrix([['1', '.3963377774', '.2158037573'],
                       ['1', '-.1055613458', '-.0638541728'],
                       ['1', '-.0894841775', '-1.2914855480']])
    lms_to_rgb = matrix([['4.0767416621', '-3.3077115913', '.2309699292'],
                         ['-1.2684380046', '2.6097574011', '-.3413193965'],
                         ['-.0041960863', '-.7034186147', '1.7076147010']])
    cases = []
    for source, linear in (
        ('mixed-spaces-rgb.ppm', [[decode(v, 65535) for v in row] for row in rgb]),
        ('mixed-spaces-xyz.tif', xyz_as_rgb),
    ):
        lab = [[D(str(x)) for x in opponents(row)[0]] for row in linear]
        centers = [lab[0], centroid(lab[1:])]
        # Seed 4 reaches this stable split. Certify its membership without
        # depending on the order in which the solver emits the two entries.
        assert [int(distance(p, centers[0]) > distance(p, centers[1]))
                for p in lab] == [0, 1, 1, 1, 1]
        palette = [apply(lms_to_rgb, [x**3 for x in apply(lms_root, center)])
                   for center in centers]
        owners = diffuse(linear, palette, bounded=True)
        assert owners == [0, 1, 0, 1, 1]
        assert owners != diffuse(lab, centers)
        assert owners != [int(distance(p, palette[0]) >
                              distance(p, palette[1])) for p in linear]
        assert wire_color(palette[1]) != wire_color(centroid(linear[1:]))
        cases.append(dict(name=source.rsplit('.', 1)[0], source=source,
                          width=5, height=1, source_width=5, scale=1,
                          hidden=[0]*5, p2=0, owners=owners,
                          wire=[wire_color(palette[i]) for i in owners]))
    assert cases[0]['wire'] == cases[1]['wire']
    return cases, fixtures


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
        # The negotiated sRGB route clips the same stored P3 ICC witness
        # in linear sRGB before applying its transfer function.
        cases.append(dict(name='png-p3-16-rgb-output-srgb',
                          source=source['source'], width=4, height=2,
                          source_width=4, scale=1, hidden=[0] * 8, p2=0,
                          linear=source['linear'],
                          wire=[wire_color(rgb) for rgb in source['linear']]))
        cli_space_first = len(cases)
        cases.extend(cli_space_cases())
        mixed_cases, mixed_fixtures = mixed_spaces_cases()
        cases.extend(mixed_cases)

    header = ('/* Generated independently; do not copy encoder output here. */\n'
              f'#define CLI_WIRE_PNM_FIRST {cli_space_first}\n'
              'static struct {\n'
              '    char const *name;\n'
              '    int width, height, source_width, scale, count, p2;\n'
              '    unsigned char hidden[8];\n'
              '    unsigned char rgb[8][3];\n'
              '} const color_cli_wire[] = {\n')
    for case in cases:
        fields = [case[k] for k in ('width', 'height', 'source_width', 'scale')]
        fields += [len(case['wire']), case['p2']]
        header += '    { "' + case['name'] + '", '
        header += ', '.join(map(str, fields)) + ',\n'
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
                         'matrix before the final output gamut clamp. Inline '
                         'PNM cases use independent centroid means, exact '
                         'nearest-color references and one raster FS tap.',
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
            'p3-palette.h': palette_header.encode(), **mixed_fixtures}


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
