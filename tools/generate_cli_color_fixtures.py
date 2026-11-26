#!/usr/bin/env python3
"""Derive CLI wire expectations from original samples and independent CMS data."""

import argparse
from decimal import Decimal as D, localcontext
from itertools import combinations
import hashlib
import json
from pathlib import Path
import struct
import zlib

import tiff_managed_reference as reference
from generate_builtin_native_fixtures import XTERM16


INPUTS = ('png-samples.json', 'png-p3-16-rgba.png', 'tiff-layout.json',
          'tiff-layout-rgb16-o6.tif', 'display-p3.icc', 'tiff-ycbcr.json',
          'png-p3-8-rgb.png', 'png-p3-16-rgb.png', 'png-raw-8-rgb.png',
          'icc-center-samples.ppm', 'icc-medoids-samples.ppm',
          'icc-heckbert-samples.ppm')


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


def wire_color(linear, maximum=100):
    result = []
    for component in linear:
        value = min(D(1), max(D(0), D(str(component))))
        gamma = (D('12.92') * value if value <= D('.0031308') else
                 D('1.055') * value ** (D(1) / D('2.4')) - D('.055'))
        scaled = gamma * maximum
        rounded = int(scaled + D('.5'))
        # This is a fixture-selection margin, not an implementation tolerance.
        # Stay away from wire half steps; runtime assertions remain exact.
        assert abs(scaled - (D(rounded) - D('.5'))) > D('.01')
        assert abs(scaled - (D(rounded) + D('.5'))) > D('.01')
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


def squared_distance(a, b):
    return sum((x - y) ** 2 for x, y in zip(a, b))


def solver_groups(model, points):
    """Return sample indices contributing to each of the two representatives."""
    if model == 'heckbert':
        # Equal populations, norm split, median cut, distinct-color average.
        spans = [max(axis) - min(axis) for axis in zip(*points)]
        axis = spans.index(max(spans))
        assert spans.count(spans[axis]) == 1
        order = sorted(range(len(points)), key=lambda i: points[i][axis])
        mid = len(points) // 2
        assert points[order[mid - 1]][axis] < points[order[mid]][axis]
        return sorted([sorted(order[:mid]), sorted(order[mid:])])

    # Exhaust all 28 sample pairs. The chosen witnesses have distinct best
    # scores. This characterizes these inputs, not a global solver guarantee.
    scores = []
    for i, j in combinations(range(len(points)), 2):
        residuals = [min(squared_distance(p, points[i]),
                         squared_distance(p, points[j])) for p in points]
        score = max(residuals) if model == 'center' else sum(residuals)
        scores.append((score, (i, j)))
    scores.sort()
    assert scores[1][0] > scores[0][0]
    if model == 'medoids':
        # From any other pair at least one PAM swap lowers the objective.
        for cost, pair in scores[1:]:
            assert any(other < cost and set(pair).intersection(candidate)
                       for other, candidate in scores)
    return [[i] for i in scores[0][1]]


FS_TAPS = ((1, 0, 7), (-1, 1, 3), (0, 1, 5), (1, 1, 1))


def raster_owners(points, palette, taps=FS_TAPS):
    """Independent two-row, four-column raster FS in the supplied coordinates."""
    working = [point[:] for point in points]
    owners, margins = [], []
    for index, point in enumerate(working):
        distances = [squared_distance(point, color) for color in palette]
        owner = distances.index(min(distances))
        margin = abs(distances[0] - distances[1])
        assert margin > D('.00001')
        owners.append(owner)
        margins.append(str(margin))
        error = [x - y for x, y in zip(point, palette[owner])]
        for dx, dy, numerator in taps:
            x, y = index % 4 + dx, index // 4 + dy
            if 0 <= x < 4 and 0 <= y < 2:
                neighbor = working[y * 4 + x]
                for c in range(3):
                    neighbor[c] += error[c] * numerator / 16
    return owners, margins


def cli_space_cases(inputs):
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

    # Solve the two-center problem directly: for three equal-weight samples
    # the minimum SSE pairs the two closest points. Read the actual colorants
    # and parametric TRCs from the file, including fixed-point rounding.
    from generate_color_fixtures import opponents
    samples = [[241, 168, 181], [88, 220, 201], [147, 61, 180]]
    tags = reference.tags(inputs['display-p3.icc'])
    matrix = list(zip(*[[reference.F(v, 65536) for v in
                         struct.unpack_from('>3i', tags[k], 8)]
                        for k in (b'rXYZ', b'gXYZ', b'bXYZ')]))
    to_linear = reference.matmul(reference.D, matrix)
    to_device = reference.inv(to_linear)

    def decimal(x):
        return D(x.numerator) / D(x.denominator)

    def encode_device(tag, value):
        assert tag[:4] == b'para' and struct.unpack_from('>H', tag, 8) == (4,)
        g, a, b, c, d, e, f = [D(n) / 65536
                               for n in struct.unpack_from('>7i', tag, 12)]
        if value < c * d + f:
            return (value - f) / c
        return ((value - e) ** (1 / g) - b) / a

    def closest(points):
        return min((sum((x-y)**2 for x, y in zip(points[i], points[j])), i, j)
                   for i in range(3) for j in range(i+1, 3))[1:]

    curves = [tags[k] for k in (b'rTRC', b'gTRC', b'bTRC')]

    def device_rgb(rgb):
        values = reference.mv(to_device, list(map(reference.F, rgb)))
        return [encode_device(tag, decimal(x)) for tag, x in zip(curves, values)]

    def linear_rgb(point):
        values = [reference.curve(tag, x) for tag, x in zip(curves, point)]
        return [decimal(v) for v in reference.mv(to_linear, values)]

    def profile_centroids(samples):
        linear = [coordinates(rgb, True) for rgb in samples]
        device = list(map(device_rgb, linear))
        assert closest(device) == (0, 1)
        centers = [[(x+y)/2 for x, y in zip(device[0], device[1])], device[2]]
        palette = list(map(linear_rgb, centers))
        wire = [wire_color(rgb) for rgb in palette]
        return linear, centers, palette, wire

    linear, centers, palette, wire = profile_centroids(samples)
    assert closest(linear) != (0, 1)
    for name, working in [('linear', lambda rgb: rgb),
                          ('oklab', lambda rgb: opponents(rgb)[0])]:
        owners = [nearest(working(rgb), list(map(working, palette)))[0]
                  for rgb in linear]
        assert owners == [0, 0, 1]
        yield case('pnm-cluster-icc-' + name, samples, owners=owners,
                   cluster_members=[[0, 1], [2]], wire=[wire[i] for i in owners])

    # With the same device as destination, nearest wire registers minimize
    # RGB distance componentwise. Reversible snap rounds through RGB8 first.
    def register(x, mode):
        if mode == 'reversible':
            byte = int(x * 255 + D('.5'))
            return (byte * 100 + 127) // 255
        return int(x * 100 + D('.5'))

    for mode in ('unsnapped', 'reversible'):
        codes = [[register(x, mode) for x in center] for center in centers]
        yield case('pnm-cluster-device-' + mode, samples, owners=owners,
                   wire=[codes[i] for i in owners])

    # Separated warm and cyan groups keep the optimum stable when automatic
    # binning changes point order and the solver initializes from bins.
    samples = [[223, 31, 25], [72, 57, 29], [16, 169, 221]]
    linear, centers, palette, wire = profile_centroids(samples)
    owners = [nearest(rgb, palette)[0] for rgb in linear]
    assert owners == [0, 0, 1]
    yield case('pnm-cluster-icc-binning', samples, owners=owners,
               wire=[wire[i] for i in owners])

    for model in ('center', 'medoids', 'heckbert'):
        name = 'icc-' + model + '-linear'
        filename = 'icc-' + model + '-samples.ppm'
        words = inputs[filename].split()
        assert words[:4] == [b'P3', b'4', b'2', b'255'] and len(words) == 28
        linear = [coordinates(map(int, words[i:i + 3]), True)
                  for i in range(4, len(words), 3)]
        points = list(map(device_rgb, linear))
        groups = solver_groups(model, points)
        assert groups != solver_groups(model, linear)
        centers = [[sum(points[i][c] for i in group) / len(group)
                    for c in range(3)] for group in groups]
        palette = list(map(linear_rgb, centers))
        wire = list(map(wire_color, palette))
        owners = [min(range(2), key=lambda i: squared_distance(p, palette[i]))
                  for p in linear]
        assert set(owners) == {0, 1}
        base = dict(source=filename, width=4, height=2, source_width=4,
                    scale=1, hidden=[0]*8, p2=0, representative_samples=groups)
        lookup = dict(base, name=name, owners=owners, wire=[wire[i] for i in owners])
        if model == 'heckbert':
            lookup['export_srgb8'] = [wire_color(rgb, 255) for rgb in palette]
        yield lookup
        if model == 'heckbert':
            fs, margins = raster_owners(linear, palette)
            assert fs != owners
            assert fs != raster_owners(linear, palette, taps=FS_TAPS[:1])[0]
            assert fs != raster_owners(points, centers)[0]
            yield dict(base, name=name + '-fs', owners=fs,
                       fs_lookup_distance_margins=margins,
                       wire=[wire[i] for i in fs])

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


def icc_register_cases():
    from generate_color_fixtures import D50, SRGB, adapt, apply, chunk, fixed
    from generate_color_fixtures import affine_profile, inverse, opponents

    input_profile = affine_profile(16384)

    def linear(code, maximum):
        return ((D(code) / maximum + D('.055')) / D('1.055')) ** D('2.4')

    def case(name, source, wire, **observations):
        return dict(name=name, source=source, width=len(wire), height=1,
                    source_width=len(wire), scale=1, hidden=[0]*len(wire),
                    p2=0, wire=wire, **observations)

    # The input ICC gives Y=(1+3x)/4. BPC maps that to (1+x)/2.
    # The destination ICC interprets code k as Y=1/2+k/200.
    nominal = [linear(v, 255) for v in (200, 209)]
    codes = [int((value - D('.5')) * 200 + D('.5')) for value in nominal]
    emitted = [D('.5') + D(code)/200 for code in codes]
    midpoint = sum(emitted)/2
    nominal_midpoint = sum(nominal)/2
    cases = []
    fixtures = {'icc-register-fixed.gpl':
                b'GIMP Palette\nName: ICC test\n200 200 200\n209 209 209\n'}
    for bpc, words in enumerate(((31352, 30583), (14260, 13107))):
        name = f'icc-register-bpc{bpc}.png'
        scanline = bytes(1) + b''.join(struct.pack('>3H', w, w, w) for w in words)
        fixtures[name] = b'\x89PNG\r\n\x1a\n' + b''.join((
            chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 1, 16, 2, 0, 0, 0)),
            chunk(b'iCCP', b'Affine input\0\0' + zlib.compress(input_profile)),
            chunk(b'IDAT', zlib.compress(scanline)), chunk(b'IEND', b''),
        ))
        first, second = [(1 + D(w)/65535)/2 if bpc
                         else (1 + 3*D(w)/65535)/4 for w in words]
        corrected = second + (first - emitted[0])*D(7)/16
        old_corrected = second + (first - nominal[1])*D(7)/16
        assert first - nominal_midpoint > D('.001')
        assert midpoint - first > D('.001') and second < nominal_midpoint
        assert corrected - midpoint > D('.002')
        assert old_corrected < nominal_midpoint
    cases.append(case('icc-register-nearest', 'icc-register-bpc{0,1}.png',
                      [[codes[0]]*3, [codes[0]]*3], indices=[0, 0]))
    cases.append(case('icc-register-fs', 'icc-register-bpc{0,1}.png',
                      [[codes[0]]*3, [codes[1]]*3], indices=[0, 1]))

    # The affine profile has a diagonal D50 matrix. Evaluate its independently
    # rounded code and all six axial neighbors in the selected coordinates.
    rgb_to_pcs = list(map(list, zip(*(adapt(col) for col in zip(*SRGB)))))
    pcs_to_rgb = inverse(rgb_to_pcs)
    white = [D(struct.unpack('>i', fixed(v))[0])/65536 for v in D50]
    samples = [(51992, 61688, 54940), (54000, 54000, 54000)]
    name = 'icc-register-centroid.ppm'
    fixtures[name] = b'P6\n2 1\n65535\n' + b''.join(
        struct.pack('>3H', *row) for row in samples)
    for space in ('linear', 'oklab'):
        def coordinates(rgb):
            return [D(str(v)) for v in opponents(rgb)[0]] if space == 'oklab' else rgb
        wire = []
        for row in samples:
            rgb = [linear(v, 65535) for v in row]
            target = coordinates(rgb)
            base = [int((x/w - D('.5'))*200 + D('.5'))
                    for x, w in zip(apply(rgb_to_pcs, rgb), white)]
            candidates = [base]
            for c in range(3):
                for delta in (-1, 1):
                    next_code = base.copy()
                    next_code[c] += delta
                    assert 0 <= next_code[c] <= 100
                    candidates.append(next_code)
            scores = []
            for code in candidates:
                pcs = [w*(D('.5') + D(k)/200) for w, k in zip(white, code)]
                observed = coordinates(apply(pcs_to_rgb, pcs))
                scores.append((sum((a-b)**2 for a, b in zip(target, observed)), code))
            scores.sort()
            assert scores[1][0] - scores[0][0] > D('1e-7')
            wire.append(scores[0][1])
        cases.append(case('icc-register-centroid-' + space, name, wire,
                          source_words=samples))
    assert cases[-1]['wire'][0] != cases[-2]['wire'][0]
    return cases, fixtures


def narrow_cluster_profile(profile):
    """Contract the linear P3 primaries towards white at unchanged TRCs."""
    blob = bytearray(profile)
    offsets = {}
    for index in range(struct.unpack_from('>I', blob, 128)[0]):
        key, offset, size = struct.unpack_from('>4sII', blob, 132 + 12 * index)
        offsets[key] = offset
    primaries = [offsets[key] + 8 for key in (b'rXYZ', b'gXYZ', b'bXYZ')]
    columns = [struct.unpack_from('>3i', blob, offset) for offset in primaries]
    white = list(map(sum, zip(*columns)))
    for offset, column in zip(primaries, columns):
        # A half-strength primary plus half the neutral, in stored units.
        struct.pack_into('>3i', blob, offset, *((3*c+w+3)//6
                                               for c, w in zip(column, white)))
    return bytes(blob)


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
        owners, fs_margins = raster_owners(working, working_palette)
        assert all(D(margin) > D('.001') for margin in fs_margins)
        painted = [palette[i] for i in owners]
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
        cases.extend(cli_space_cases(inputs))
        mixed_cases, mixed_fixtures = mixed_spaces_cases()
        cases.extend(mixed_cases)
        icc_cases, icc_fixtures = icc_register_cases()
        cases.extend(icc_cases)

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
    export = next(c['export_srgb8'] for c in cases if c['name'] == 'icc-heckbert-linear')
    pal = 'JASC-PAL\n0100\n2\n' + ''.join('%d %d %d\n' % tuple(row) for row in export)
    return {'narrow-p3-cluster.icc': narrow_cluster_profile(inputs['display-p3.icc']),
            'cli-color-wire.h': header.encode(),
            'cli-color-wire.json': (json.dumps(record, indent=2) + '\n').encode(),
            'p3-palette.h': palette_header.encode(),
            'icc-heckbert-srgb8.pal': pal.encode(), **mixed_fixtures,
            **icc_fixtures}


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
