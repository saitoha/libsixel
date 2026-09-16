#!/usr/bin/env python3
"""Literal legacy TIFF witnesses and original-sample tables, without a CMM."""
import hashlib
import json
import struct


def directory(tags):
    start = 8 + 2 + len(tags) * 12 + 4
    entries, extra = [], bytearray()
    for tag, (kind, values) in sorted(tags.items()):
        blob = values if kind == 7 else struct.pack(
            '<' + {3: 'H', 4: 'I'}[kind] * len(values), *values)
        if len(blob) <= 4:
            field = blob.ljust(4, b'\0')
        else:
            extra.extend(bytes(-len(extra) % 2))
            field = struct.pack('<I', start + len(extra))
            extra.extend(blob)
        entries.append(struct.pack('<HHI', tag, kind, len(values)) + field)
    return (b'II*\0' + struct.pack('<IH', 8, len(entries)) +
            b''.join(entries) + bytes(4) + extra)


def packed_row(values, bits):
    if bits >= 8:
        return struct.pack('<' + ('B' if bits == 8 else 'H') * len(values),
                           *values)
    data = bytearray((len(values) * bits + 7) // 8)
    for i, value in enumerate(values):
        data[i * bits // 8] |= value << (8 - bits - i * bits % 8)
    return bytes(data)


def literal(item, profile):
    rows = item['samples']
    width, height = item['width'], item['height']
    bits, channels = item['bits'], item['channels']
    tags = {256: (4, [width]), 257: (4, [height]),
            258: (3, [bits] * channels), 259: (3, [1]),
            262: (3, [item['photo']]), 274: (3, [1]),
            277: (3, [channels]), 284: (3, [item['planar']]),
            339: (3, [1] * channels), 34675: (7, profile)}
    if item['alpha']:
        tags[338] = (3, [item['alpha']])
    if item['photo'] == 3:
        tags[320] = (3, [c[ch] for ch in range(3)
                        for c in item['colormap']])
    if item['photo'] == 6:
        tags[530] = (3, [1, 1])
    parts = []
    for channel in range(channels if item['planar'] == 2 else 1):
        part = bytearray()
        for y in range(height):
            row = rows[y * width:(y + 1) * width]
            values = ([pixel[channel] for pixel in row]
                      if item['planar'] == 2 else sum(row, []))
            part.extend(packed_row(values, bits))
        parts.append(bytes(part))
    tags.update({273: (4, [0] * len(parts)), 278: (4, [height]),
                 279: (4, [len(part) for part in parts])})
    start = len(directory(tags))
    offsets = []
    for part in parts:
        offsets.append(start)
        start += len(part)
    tags[273] = (4, offsets)
    return directory(tags) + b''.join(parts)


def build(p3, gray):
    rgb = [[0, 1, 256], [257, 32897, 16449], [65535, 32768, 8225],
           [129, 65534, 128], [32767, 128, 65535], [32896, 16448, 8224]]
    colormap = [[0, 1, 255], [256, 257, 258], [32897, 16449, 8225],
                [65535, 65534, 65533]] + [[0, 0, 0]] * 12
    specs = [
        ('rgb16-planar', 2, 16, 2, rgb, 3, 0),
        ('black4-levels', 1, 4, 1, [[v] for v in range(16)], 16, 0),
        ('palette4-map16', 3, 4, 1, [[v] for v in [2, 0, 3, 1, 3, 2]], 3, 0),
        ('ycbcr8-111', 6, 8, 1, [[80, 90, 200], [0, 128, 128],
         [255, 128, 128], [128, 128, 128], [150, 200, 60], [200, 16, 240]], 3, 0),
        ('rgb8-planar-alpha', 2, 8, 2, [[64, 32, 16, 128],
         [128, 64, 32, 255], [0, 0, 0, 0], [1, 0, 0, 1],
         [32, 16, 8, 64], [255, 128, 64, 255]], 3, 1)]
    cases = []
    for name, photo, bits, planar, samples, width, alpha in specs:
        cases.append(dict(name=name, photo=photo, bits=bits, planar=planar,
            samples=samples, width=width, height=len(samples) // width,
            channels=len(samples[0]), alpha=alpha, mismatch=0,
            colormap=colormap if photo == 3 else []))
    cases += [dict(item, name=item['name'] + '-mismatch', mismatch=1)
              for item in cases[:4]]
    outputs, fixtures = {}, []
    header = '''/* Literal original samples; no CMM output is used here. */
#if HAVE_LIBTIFF
static struct {
    char const *name;
    int photo, bits, planar, width, height, channels, alpha, mismatch;
    unsigned int source[64];
    unsigned int colormap[48];
} const tl_cases[] = {
'''
    for item in cases:
        is_gray = (item['photo'] == 1) != bool(item['mismatch'])
        profile = gray if is_gray else p3
        name = 'tiff-legacy-' + item['name'] + '.tif'
        data = literal(item, profile)
        outputs[name] = data
        fixtures.append(dict(file=name, sha256=hashlib.sha256(data).hexdigest(),
                             profile_sha256=hashlib.sha256(profile).hexdigest()))
        fields = [item[k] for k in ['photo', 'bits', 'planar', 'width',
                                    'height', 'channels', 'alpha', 'mismatch']]
        header += '    { "%s",\n        %s, {\n' % (
            item['name'], ', '.join(map(str, fields)))
        for key in ['samples', 'colormap']:
            values = sum(item[key], []) or [0]
            header += '\n'.join('        ' + ', '.join(map(str, values[i:i+8])) + ','
                                for i in range(0, len(values), 8))
            header += '\n    }' + (', {\n' if key == 'samples' else ' },\n')
    header += '};\n#endif\n'
    outputs['tiff-legacy.h'] = header.encode()
    outputs['tiff-legacy.json'] = (json.dumps(dict(
        generator='tools/generate_tiff_legacy_fixtures.py',
        oracle='Literal original samples and TIFF tags; no CMM or TIFF writer. '
               'C tests verify decoded original strips independently before '
               'using the current libtiff RGBA control for transaction checks.',
        cases=cases, fixtures=fixtures), indent=2) + '\n').encode()
    return outputs
