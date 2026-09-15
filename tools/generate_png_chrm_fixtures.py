#!/usr/bin/env python3
"""Independent PNG cHRM integer-grid specimens and sample expectations."""

from decimal import Decimal as N, localcontext
from fractions import Fraction
import hashlib
import itertools
import json
import struct
import zlib


def chunks(data):
    result = []
    offset = 8
    while offset < len(data):
        size = int.from_bytes(data[offset:offset + 4], 'big')
        result.append((data[offset + 4:offset + 8],
                       data[offset + 8:offset + 8 + size]))
        offset += size + 12
    assert offset == len(data)
    return result


def chunk(kind, value):
    return (struct.pack('>I', len(value)) + kind + value
            + struct.pack('>I', zlib.crc32(kind + value)))


def multiply(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(len(b)))
             for j in range(len(b[0]))] for i in range(len(a))]


def inverse(matrix):
    rows = [row[:] + [N(i == j) for j in range(3)]
            for i, row in enumerate(matrix)]
    for col in range(3):
        pivot = max(range(col, 3), key=lambda i: abs(rows[i][col]))
        rows[col], rows[pivot] = rows[pivot], rows[col]
        value = rows[col][col]
        assert value != 0
        rows[col] = [v / value for v in rows[col]]
        for i in range(3):
            if i != col:
                value = rows[i][col]
                rows[i] = [rows[i][j] - value * rows[col][j]
                           for j in range(6)]
    return [row[3:] for row in rows]


def source_matrix(grid):
    pairs = [(N(grid[i]) / 100000, N(grid[i + 1]) / 100000)
             for i in range(0, 8, 2)]
    white = pairs[0]
    primary = pairs[1:]
    unscaled = [[x / y for x, y in primary], [N(1)] * 3,
                [(1 - x - y) / y for x, y in primary]]
    scale = multiply(inverse(unscaled), [[white[0] / white[1]], [N(1)],
                     [(1 - white[0] - white[1]) / white[1]]])
    return [[unscaled[i][j] * scale[j][0] for j in range(3)]
            for i in range(3)]


def f32(value):
    return struct.unpack('<f', struct.pack('<f', float(value)))[0]


def srgb(value):
    return (value / N('12.92') if value <= N('.04045') else
            ((value + N('.055')) / N('1.055')) ** N('2.4'))


def icc_definition(profile, destination):
    tags = {}
    for i in range(int.from_bytes(profile[128:132], 'big')):
        pos = 132 + 12 * i
        offset, size = struct.unpack('>2I', profile[pos + 4:pos + 12])
        tags[profile[pos:pos + 4]] = profile[offset:offset + size]
    def fixed(data):
        return N(int.from_bytes(data, 'big', signed=True)) / 65536
    matrix = [[fixed(tags[c + b'XYZ'][8 + 4 * i:12 + 4 * i])
               for c in [b'r', b'g', b'b']] for i in range(3)]
    powers = [fixed(tags[c + b'TRC'][12:16]) for c in [b'r', b'g', b'b']]
    bradford = [list(map(N, row.split())) for row in [
        '.8951 .2664 -.1614', '-.7502 1.7135 .0367', '.0389 -.0685 1.0296']]
    source = multiply(bradford, [[N('.9642')], [N(1)], [N('.8249')]])
    target = multiply(bradford, [[N('.3127') / N('.329')], [N(1)],
                                [N('.3583') / N('.329')]])
    diagonal = [[target[i][0] / source[i][0] if i == j else N(0)
                 for j in range(3)] for i in range(3)]
    return multiply(inverse(destination), multiply(inverse(bradford),
                    multiply(diagonal, multiply(bradford, matrix)))), powers


def primary_guard_specs():
    """Classify original homogeneous columns by rational elimination."""
    triples = [[18750, 8, 18044, 9, 17338, 10],
               [40210, 8, 39976, 9, 39742, 10]]
    specs = []
    for number, primary in enumerate(triples):
        for white, xy in [('d65', [31270, 32900]), ('d50', [34570, 35850]),
                          ('near-cone', [14071, 2978])]:
            for depth in [8, 16]:
                for shape in ['rgb', 'rgba', 'file-bg']:
                    for mode in ['gama-chrm', 'chrm-only']:
                        specs.append((f'{depth}-{shape}-collinear-{number}-{white}-{mode}',
                            depth, shape, xy + primary, False, mode, None))
    for number, primary in enumerate(triples):
        for depth, shape in [(8, 'rgb'), (16, 'file-bg')]:
            for mode in ['srgb-chrm', 'icc-chrm', 'icc-srgb', 'triple',
                         'bad-icc', 'gama-only']:
                specs.append((f'{depth}-{shape}-collinear-{number}-priority-{mode}',
                    depth, shape, [34570, 35850] + primary, False, mode, None))
    for number, primary in enumerate(triples):
        pairs = [primary[i:i+2] for i in range(0, 6, 2)]
        for order, permutation in enumerate(itertools.permutations(pairs)):
            if order:
                specs.append((f'8-rgb-collinear-{number}-permutation-{order}-gama-chrm',
                    8, 'rgb', [34570, 35850] + sum(permutation, []),
                    False, 'gama-chrm', None))
    for label, primary in [('one', [100, 100, 101, 100, 100, 101]),
                            ('large', [0, 1, 99999, 1, 0, 100000])]:
        for sign, values in [('positive', primary),
                             ('negative', primary[2:4] + primary[:2] + primary[4:])]:
            specs.append((f'8-rgb-area-{sign}-{label}-chrm-only', 8, 'rgb',
                [31270, 32900] + values, True, 'chrm-only', None))
    # Independent rank proof: eliminate exact homogeneous XYZ columns. This
    # does not copy the production two-product signed-area expression.
    for _, _, _, grid, valid, _, _ in specs:
        pairs = list(zip(grid[2::2], grid[3::2]))
        rows = [[Fraction(v) for v in row] for row in zip(
            *[(x, y, 100000-x-y) for x, y in pairs])]
        rank = 0
        for col in range(3):
            pivot = next((i for i in range(rank, 3) if rows[i][col]), None)
            if pivot is None:
                continue
            rows[rank], rows[pivot] = rows[pivot], rows[rank]
            scale = rows[rank][col]
            rows[rank] = [v / scale for v in rows[rank]]
            for i in range(rank+1, 3):
                scale = rows[i][col]
                rows[i] = [a-scale*b for a, b in zip(rows[i], rows[rank])]
            rank += 1
        assert (rank == 3) == valid
    return specs


def build(palette_png):
    """Build from original integers and an independently generated ICC seed."""
    iccp = next(value for kind, value in chunks(palette_png) if kind == b'iCCP')
    profile = zlib.decompress(iccp[iccp.index(0) + 2:])
    samples = {
        8: [[0, 0, 0, 0], [255, 255, 255, 255], [10, 20, 30, 40],
            [50, 100, 150, 85], [200, 80, 30, 192], [255, 0, 0, 255],
            [0, 255, 0, 0], [0, 0, 255, 255]],
        16: [[0, 0, 0, 0], [65535, 65535, 65535, 65535],
             [2571, 5141, 7711, 10281], [12851, 25701, 38551, 21846],
             [51401, 20561, 7711, 49345], [65535, 0, 0, 65535],
             [0, 65535, 0, 0], [0, 0, 65535, 65535]]}
    specifications = []
    for depth in [8, 16]:
        for shape in ['rgb', 'rgba', 'file-bg']:
            for boundary, red_y in [('inside', 31999), ('zero', 32000),
                                    ('outside', 32001)]:
                grid = [31270, 32900, 68000, red_y, 26500, 69000, 15000, 6000]
                for mode in ['gama-chrm', 'chrm-only', 'srgb-chrm', 'icc-chrm',
                             'icc-srgb', 'triple', 'bad-icc', 'gama-only',
                             'untagged']:
                    if boundary != 'zero' and mode not in ['gama-chrm', 'chrm-only']:
                        continue
                    specifications.append((f'{depth}-{shape}-{boundary}-{mode}',
                        depth, shape, grid, boundary != 'outside', mode, None))
    for point in ['green', 'blue']:
        for depth, shape in [(8, 'rgb'), (16, 'file-bg')]:
            for mode in ['gama-chrm', 'chrm-only']:
                for boundary, delta in [('inside', -1), ('zero', 0), ('outside', 1)]:
                    pairs = [[68000, 32000 + delta], [26500, 69000], [15000, 6000]]
                    pairs = ([pairs[2], pairs[0], pairs[1]] if point == 'green'
                             else [pairs[1], pairs[2], pairs[0]])
                    specifications.append((f'{depth}-{shape}-{point}-{boundary}-{mode}',
                        depth, shape, [31270, 32900] + sum(pairs, []),
                        boundary != 'outside', mode, None))
    for kind in ['red-x-over', 'red-y-over', 'white-x-over', 'red-y-zero',
                 'white-y-zero', 'singular', 'bad-crc', 'bad-length', 'signbit',
                 'intmin', 'max-signed']:
        for depth, shape in [(8, 'rgb'), (16, 'file-bg')]:
            grid = [31270, 32900, 68000, 32000, 26500, 69000, 15000, 6000]
            changes = {'red-x-over': (2, 100001), 'red-y-over': (3, 100001),
                       'white-x-over': (0, 100001), 'red-y-zero': (3, 0),
                       'white-y-zero': (1, 0), 'signbit': (2, 0xffffffff),
                       'intmin': (2, 0x80000000), 'max-signed': (2, 0x7fffffff)}
            if kind in changes:
                pos, value = changes[kind]
                grid[pos] = value
            if kind == 'singular':
                grid[2:] = [64000, 33000] * 3
            specifications.append((f'{depth}-{shape}-{kind}-gama-chrm',
                depth, shape, grid, False, 'gama-chrm', kind))
    for depth, shape in [(8, 'rgb'), (16, 'file-bg')]:
        specifications.append((f'{depth}-{shape}-singular-chrm-only', depth,
            shape, [31270, 32900] + [64000, 33000] * 3, False, 'chrm-only', None))
    specifications.extend(primary_guard_specs())
    outputs, records = {}, []
    with localcontext() as context:
        context.prec = 100
        destination = source_matrix([31270, 32900, 64000, 33000, 30000, 60000, 15000, 6000])
        icc_matrix, powers = icc_definition(profile, destination)
        for name, depth, shape, grid, valid, mode, corruption in specifications:
            raw = b'\0' + b''.join(struct.pack('>H', value) if depth == 16 else bytes([value])
                for pixel in samples[depth] for value in pixel[:3 if shape == 'rgb' else 4])
            pieces = [(b'IHDR', struct.pack('>IIBBBBB', 8, 1, depth,
                                           2 if shape == 'rgb' else 6, 0, 0, 0))]
            if mode in ['icc-chrm', 'icc-srgb', 'triple', 'bad-icc']:
                pieces.append((b'iCCP', b'bad\0\0' + zlib.compress(b'invalid')
                               if mode == 'bad-icc' else iccp))
            if mode in ['srgb-chrm', 'icc-srgb', 'triple']:
                pieces.append((b'sRGB', b'\0'))
            if mode not in ['chrm-only', 'untagged']:
                pieces.append((b'gAMA', struct.pack('>I', 25000)))
            if mode not in ['gama-only', 'untagged', 'icc-srgb']:
                data = struct.pack('>8I', *grid)
                pieces.append((b'cHRM', data[:31] if corruption == 'bad-length' else data))
            if shape == 'file-bg':
                pieces.append((b'bKGD', struct.pack('>3H', *samples[depth][4][:3])))
            pieces.extend([(b'IDAT', zlib.compress(raw)), (b'IEND', b'')])
            packed = []
            for kind, value in pieces:
                data = chunk(kind, value)
                if kind == b'cHRM' and corruption == 'bad-crc':
                    data = data[:-1] + bytes([data[-1] ^ 1])
                packed.append(data)
            data = b'\x89PNG\r\n\x1a\n' + b''.join(packed)
            filename = 'png-chrm-' + name + '.png'
            outputs[filename] = data
            matrix = multiply(inverse(destination), source_matrix(grid)) if valid else None
            expected = []
            peak = (1 << depth) - 1
            for interpretation in ['srgb', 'gama', 'chrm-srgb', 'chrm-gama', 'icc']:
                values = []
                for pixel in samples[depth]:
                    rgb = [N(v) / peak for v in pixel[:3]]
                    if interpretation == 'icc':
                        rgb = [row[0] for row in multiply(icc_matrix,
                               [[v ** power] for v, power in zip(rgb, powers)])]
                    else:
                        rgb = [v ** 4 if interpretation.endswith('gama') else srgb(v) for v in rgb]
                        if interpretation.startswith('chrm') and matrix is not None:
                            rgb = [row[0] for row in multiply(matrix, [[v] for v in rgb])]
                    values.append(rgb)
                if shape == 'file-bg':
                    background = values[4][:]
                    values = [[v * N(pixel[3]) / peak + bg * (1 - N(pixel[3]) / peak)
                               for v, bg in zip(rgb, background)] if pixel[3] else rgb
                              for pixel, rgb in zip(samples[depth], values)]
                expected.append([f32(value) for pixel in values for value in pixel])
            records.append(dict(name=name, file=filename, depth=depth, shape=shape,
                mode=mode, grid=grid, valid=valid, samples=samples[depth],
                interpretations=['srgb', 'gama', 'chrm-srgb', 'chrm-gama', 'icc'],
                expected=expected, sha256=hashlib.sha256(data).hexdigest()))
            if 'collinear-' in name or '-area-' in name:
                origin = f'png-chrm-{depth}-{shape}-zero-{mode}.png'
                original = outputs[origin]
                records[-1]['provenance'] = dict(
                    origin=origin, origin_sha256=hashlib.sha256(original).hexdigest(),
                    whole_png_equal_to_origin=data == original,
                    idat_payload_equal_to_origin=
                        [v for k, v in chunks(data) if k == b'IDAT'] ==
                        [v for k, v in chunks(original) if k == b'IDAT'],
                    samples='Original integer samples; regenerated metadata and CRCs')
    header = ('/* Generated by tools/generate_png_chrm_fixtures.py. */\n'
        '#include <stddef.h>\n'
        'typedef struct {\n    unsigned char const *bytes;\n    size_t size;\n'
        '    int depth;\n    int alpha;\n    int background;\n'
        '    int valid_chrm;\n    int srgb_priority;\n    int icc_priority;\n'
        '    unsigned int samples[32];\n    float expected[5][24];\n'
        '} color_png_chrm_t;\n')
    for i, row in enumerate(records):
        data = outputs[row['file']]
        header += f'static unsigned char const color_chrm_png_{i}[] = {{\n'
        header += ''.join('    ' + ', '.join(f'0x{v:02x}' for v in data[j:j+12]) + ',\n'
                          for j in range(0, len(data), 12)) + '};\n'
    header += 'static color_png_chrm_t const color_png_chrm[] = {\n'
    for i, row in enumerate(records):
        flags = [row['depth'], row['shape'] != 'rgb', row['shape'] == 'file-bg',
                 row['valid'], row['mode'] in ['srgb-chrm', 'triple'],
                 row['mode'] in ['icc-chrm', 'icc-srgb']]
        header += (f'    {{ color_chrm_png_{i}, sizeof(color_chrm_png_{i}),\n'
                   '      ' + ', '.join(str(int(v)) for v in flags) + ',\n      {\n')
        for pixel in row['samples']:
            header += '        ' + ', '.join(str(v) for v in pixel) + ',\n'
        header += '      }, {\n'
        for expected in row['expected']:
            header += '        {\n'
            for j in range(0, 24, 2):
                header += '          ' + ', '.join(float(v).hex() + 'f' for v in expected[j:j+2]) + ',\n'
            header += '        },\n'
        header += '      } },\n'
    header += '};\n'
    outputs['png-chrm.h'] = header.encode()
    outputs['png-chrm.json'] = (json.dumps(dict(
        generator='tools/generate_png_chrm_fixtures.py', arithmetic='100-digit Decimal from original PNG integers; no product evaluator',
        profile_sha256=hashlib.sha256(profile).hexdigest(), fixtures=records), indent=2) + '\n').encode()
    return outputs
