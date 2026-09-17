#!/usr/bin/env python3
"""Literal TIFF layout witnesses, reusing the finite original-sample corpus."""
import hashlib
import json

from generate_tiff_legacy_fixtures import directory, packed_row
from generate_tiff_separate16_fixtures import float_word


def literal(item, profile):
    """Serialize original samples directly, without a TIFF writer or CMM."""
    width, height = item['width'], item['height']
    bits, samples = item['bits'], item['samples']
    channels = len(samples[0])
    planar = item['layout'] == 'planar' or item.get('separate', False)
    tiled = item['layout'] == 'tile'
    tags = {256: (4, [width]), 257: (4, [height]),
            258: (3, [bits] * channels), 259: (3, [item['compression']]),
            262: (3, [item['photo']]), 274: (3, [item['orientation']]),
            277: (3, [channels]), 284: (3, [2 if planar else 1]),
            339: (3, [1] * channels)}
    if profile is not None:
        tags[34675] = (7, profile)
    if item['alpha']:
        tags[338] = (3, [item['alpha']])
    if item['photo'] == 3:
        tags[320] = (3, [p[c] for c in range(3) for p in item['colormap']])
    if item.get('fill_order', 1) == 2:
        tags[266] = (3, [2])
    parts = []
    if tiled:
        for plane in range(channels if planar else 1):
            for top in range(0, height, 16):
                for left in range(0, width, 16):
                    rows = []
                    for y in range(16):
                        values = []
                        for x in range(16):
                            pixel = (samples[(top+y)*width+left+x]
                                     if top+y < height and left+x < width else
                                     [0] * channels)
                            values.extend([pixel[plane]] if planar else pixel)
                        rows.append(packed_row(values, bits))
                    parts.append(b''.join(rows))
        offsets_tag, sizes_tag = 324, 325
        tags.update({322: (4, [16]), 323: (4, [16])})
    else:
        for plane in range(channels if planar else 1):
            rows = []
            for y in range(height):
                pixels = samples[y*width:(y+1)*width]
                values = ([p[plane] for p in pixels] if planar else
                          sum(pixels, []))
                rows.append(packed_row(values, bits))
            parts.append(b''.join(rows))
        offsets_tag, sizes_tag = 273, 279
        tags[278] = (4, [height])
    if item.get('fill_order', 1) == 2:
        parts = [bytes(int(f'{v:08b}'[::-1], 2) for v in part) for part in parts]
    if item['compression'] == 32773:
        parts = [b''.join(bytes([len(p[i:i+128]) - 1]) + p[i:i+128]
                          for i in range(0, len(p), 128)) for p in parts]
    tags[sizes_tag] = (4, [len(p) for p in parts])
    tags[offsets_tag] = (4, [0] * len(parts))
    offset = len(directory(tags))
    offsets = []
    for part in parts:
        offsets.append(offset)
        offset += len(part)
    tags[offsets_tag] = (4, offsets)
    return directory(tags) + b''.join(parts)


def oracle(item):
    """Use exact integer ratios and explicit displayed-coordinate equations."""
    width, height, orientation = (item[k] for k in
                                   ('width', 'height', 'orientation'))
    out_width = height if orientation >= 5 else width
    out = [None] * len(item['samples'])
    alpha = [0] * len(out)
    maximum = (1 << item['bits']) - 1
    for i, pixel in enumerate(item['samples']):
        x, y = i % width, i // width
        dx, dy = [(x, y), (width-1-x, y), (width-1-x, height-1-y),
                  (x, height-1-y), (y, x), (height-1-y, x),
                  (height-1-y, width-1-x), (y, width-1-x)][orientation-1]
        index = dy * out_width + dx
        a = pixel[-1] if item['alpha'] else maximum
        alpha[index] = a
        denominator = a if item['alpha'] == 1 else maximum
        if item['photo'] == 3:
            rgb, denominator = item['colormap'][pixel[0]], 65535
        elif item['photo'] == 2:
            rgb = pixel[:3]
        else:
            rgb = [maximum - pixel[0] if item['photo'] == 0 else pixel[0]] * 3
        out[index] = [float_word(v, denominator) if denominator else 0
                      for v in rgb]
    return sum(out, []), alpha


def build(p3, gray):
    colors = {
        8: [[0, 1, 2], [17, 63, 129], [255, 128, 64], [1, 255, 31],
            [254, 0, 128], [128, 64, 32]],
        16: [[0, 1, 256], [257, 32897, 16449], [65535, 32768, 8225],
             [129, 65534, 128], [32767, 128, 65535], [32896, 16448, 8224]]}
    cases = []

    def add(name, photo, bits, layout='strip', orientation=1, **extra):
        samples = (colors[bits] if photo == 2 else
                   [[(1 << bits)-1-p[0] if photo == 0 else p[0]]
                    for p in colors[bits]]) if bits >= 8 else []
        item = dict(name=name, photo=photo, bits=bits, layout=layout,
                    orientation=orientation, samples=samples, width=3,
                    height=2, compression=1, alpha=0, colormap=[],
                    profile='match')
        item.update(extra)
        cases.append(item)

    for photo, name in [(2, 'rgb'), (1, 'black'), (0, 'white')]:
        for bits in [8, 16]:
            add(f'{name}{bits}-tile', photo, bits, 'tile')
    add('rgb8-planar', 2, 8, 'planar')
    for orientation in [5, 6, 7, 8]:
        add(f'rgb8-o{orientation}', 2, 8, orientation=orientation)
    add('rgb16-o6', 2, 16, orientation=6)
    add('white16-o8', 0, 16, orientation=8)
    for photo, name in [(1, 'black'), (0, 'white')]:
        for bits in [1, 2, 4]:
            add(f'{name}{bits}-levels', photo, bits,
                samples=[[v if photo else (1 << bits)-1-v]
                         for v in range(1 << bits)], width=1 << bits, height=1)
    add('palette4-map16', 3, 4, samples=[[v] for v in [2, 0, 3, 1, 3, 2]],
        colormap=[[0, 1, 255], [256, 257, 258], [32897, 16449, 8225],
                  [65535, 65534, 65533]] + [[0, 0, 0]] * 12)
    for photo, name in [(2, 'rgb'), (1, 'black'), (0, 'white')]:
        for bits in [8, 16]:
            add(f'{name}{bits}-tile-packbits', photo, bits, 'tile',
                compression=32773)
    add('rgb8-planar-alpha', 2, 8, 'planar', alpha=1,
        samples=[[64, 32, 16, 128], [128, 64, 32, 255], [0, 0, 0, 0],
                 [1, 0, 0, 1], [32, 16, 8, 64], [255, 128, 64, 255]])
    by_name = {item['name']: item for item in cases}
    for name in ['rgb16-tile', 'black4-levels', 'palette4-map16']:
        cases.append(dict(by_name[name], name=name+'-mismatch',
                          profile='mismatch'))
    for kind in ['absent', 'invalid']:
        cases.append(dict(by_name['rgb16-tile'], name='rgb16-tile-'+kind,
                          profile=kind))
    # Three bounded controls cover the new tile/packing loops beyond the
    # previously reviewed corpus, without creating a format cross-product.
    add('rgb16-multitile-alpha', 2, 16, 'tile', width=17, height=18,
        orientation=6, separate=True, compression=32773, alpha=1,
        samples=[colors[16][i % 6] + [[65535, 32768, 0, 1, 16384, 65535][i % 6]]
                 for i in range(17 * 18)])
    palette = [[0, 0, 0] for _ in range(65536)]
    for index, value in zip([1, 257, 65535],
                           [[256, 257, 258], [32897, 16449, 8225],
                            [65535, 65534, 65533]]):
        palette[index] = value
    add('palette16-index', 3, 16, width=3, height=1,
        samples=[[1], [257], [65535]], colormap=palette)
    cases.append(dict(by_name['white2-levels'], name='white2-fillorder2',
                      fill_order=2))
    reused = {'rgb8-planar', 'black4-levels', 'palette4-map16',
              'rgb8-planar-alpha', 'black4-levels-mismatch',
              'palette4-map16-mismatch'}
    outputs, records = {}, []
    header = '''/* Original integer samples and exact rational-to-F32 values. */
#if HAVE_LIBTIFF
typedef struct {
    char const *file;
    int photo, bits, planar, orientation, width, height, alpha, matched;
    uint16_t const *original, *coverage;
    uint32_t const *source;
} ti_case_t;
'''
    entries = []
    for number, item in enumerate(cases):
        name = item['name']
        matched = item['profile'] == 'match'
        profile = gray if ((item['photo'] in (0, 1)) !=
                           (item['profile'] == 'mismatch')) else p3
        profile = (None if item['profile'] == 'absent' else b'not-an-icc'
                   if item['profile'] == 'invalid' else profile)
        filename = ('tiff-legacy-' if name in reused else 'tiff-layout-') + name + '.tif'
        data = literal(item, profile)
        if name not in reused:
            outputs[filename] = data
        words, alpha = oracle(item)
        record = {k: v for k, v in item.items() if k != 'colormap'}
        record['selected_map'] = ([item['colormap'][pixel[0]]
                                  for pixel in item['samples']]
                                 if item['photo'] == 3 else [])
        records.append(dict(record, file=filename, reused=name in reused,
            sha256=hashlib.sha256(data).hexdigest(), source_words=words,
            coverage=alpha))
        fields = [item['photo'], item['bits'],
                  int(item['layout'] == 'planar' or item.get('separate', False)),
                  item['orientation'], item['width'], item['height'],
                  item['alpha'], int(matched)]
        arrays = [sum(item['samples'], []), alpha, words]
        for j, values in enumerate(arrays):
            fmt = (lambda v: '0x%08xu' % v) if j == 2 else str
            header += 'static %s const ti_data_%d_%d[] = {\n' % (
                'uint32_t' if j == 2 else 'uint16_t', number, j)
            header += '\n'.join('    ' + ', '.join(fmt(v) for v in values[i:i+4]) + ','
                                for i in range(0, len(values), 4)) + '\n};\n'
        entries.append('    { "%s",\n      %s,\n      ti_data_%d_0, ti_data_%d_1, ti_data_%d_2 },' %
                       (filename, ', '.join(map(str, fields)), number, number, number))
    header += 'static ti_case_t const ti_cases[] = {\n' + '\n'.join(entries)
    outputs['tiff-layout.h'] = (header + '};\n#endif\n').encode()
    outputs['tiff-layout.json'] = (json.dumps(dict(
        generator='tools/generate_tiff_layout_fixtures.py', fixtures=records),
        indent=2) + '\n').encode()
    return outputs
