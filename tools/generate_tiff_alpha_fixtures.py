#!/usr/bin/env python3
"""Literal alpha TIFFs with original-ratio and independent stored-ICC oracles."""
import hashlib
import json
import struct
from fractions import Fraction as F
import tiff_managed_reference as ref


def write_tiff(pixels, photo, icc, orientation=1, width=None, sample=1, alpha=1,
               bits=8, endian='<'):
    """Write original samples directly, without a TIFF or CMS dependency."""
    channels = len(pixels[0])
    raw = struct.pack(endian + {8: 'B', 16: 'H', 32: 'f'}[bits] *
                      (len(pixels) * channels), *sum(pixels, []))
    width = width or len(pixels)
    height = len(pixels) // width
    tags = {256: (4, [width]), 257: (4, [height]),
            258: (3, [bits] * channels), 259: (3, [1]), 262: (3, [photo]),
            273: (4, [0]), 274: (3, [orientation]), 277: (3, [channels]),
            278: (4, [height]), 279: (4, [len(raw)]), 284: (3, [1]),
            339: (3, [sample] * channels)}
    if alpha:
        tags[338] = (3, [alpha])
    if icc is not None:
        tags[34675] = (7, icc)
    start = 8 + 2 + len(tags) * 12 + 4
    entries, extra = [], bytearray()
    for tag, (kind, values) in sorted(tags.items()):
        packed = values if kind == 7 else struct.pack(
            endian + ('H' if kind == 3 else 'I') * len(values), *values)
        if len(packed) <= 4:
            field = packed.ljust(4, b'\0')
        else:
            extra.extend(bytes(-len(extra) % 2))
            field = struct.pack(endian + 'I', start + len(extra))
            extra.extend(packed)
        entries.append([tag, kind, len(values), field])
    for entry in entries:
        if entry[0] == 273:
            entry[3] = struct.pack(endian + 'I', start + len(extra))
    return ((b'II' if endian == '<' else b'MM') +
            struct.pack(endian + 'HIH', 42, 8, len(entries)) +
            b''.join(struct.pack(endian + 'HHI', t, k, n) + v
                     for t, k, n, v in entries) + bytes(4) + extra + raw)



def decode(value):
    """Signed sRGB definition evaluated independently in rational/Decimal."""
    value = F(value)
    magnitude = abs(value)
    result = (magnitude / F(323, 25) if magnitude <= F(809, 20000)
              else ref.power((magnitude + F(11, 200)) / F(211, 200),
                             F(12, 5)))
    return result if value >= 0 else -result


def encode(value):
    value = F(value)
    magnitude = abs(value)
    result = (magnitude * F(323, 25) if magnitude <= F(7827, 2500000)
              else F(211, 200) * ref.power(magnitude, F(5, 12)) - F(11, 200))
    return result if value >= 0 else -result


def f32(value):
    return struct.unpack('>f', struct.pack('>I', ref.nearest_rational(value)))[0]


def samples(photo, association, kind):
    alpha = [0, 1, 128, 255]
    if kind == 'extended':
        colors = ([[0, 0, 0], [2, 1, 0], [0, 128, 0], [0, 255, 0]]
                  if association == 1 else [[0, 255, 0]] * 4)
    elif kind == 'all255':
        colors = ([[0, 1, 2], [1, 2, 3], [128, 64, 32], [255]*3]
                  if photo == 2 else [[v] for v in [0, 1, 128, 255]])
        alpha = [255]*4
    elif photo == 2:
        colors = ([[0, 0, 0], [1, 0, 0], [64, 32, 16], [128, 64, 32]]
                  if association == 1 else [[128, 64, 32]] * 4)
    else:
        colors = ([[0], [1], [64], [128]] if association == 1
                  else [[128]] * 4)
    if kind == 'opaque':
        alpha = [255]*4
    if photo == 0:
        colors = [[255-v for v in row] for row in colors]
    return [row + [a] for row, a in zip(colors, alpha)]


def oracle(name, photo, association, pixels, profile, bits=8):
    maximum = (1 << bits) - 1
    source = []
    for row in pixels:
        alpha = row[-1]
        values = [F(0) if association == 1 and not alpha else
                  F(v if photo else maximum-v,
                    alpha if association == 1 else maximum)
                  for v in row[:-1]]
        source.extend(values if photo == 2 else values*3)
    words = [ref.nearest_rational(v) for v in source]
    values = [f32(v) for v in source]
    canonical = [ref.expected(dict(icc=profile,
        triples=[values[i:i+3] for i in range(0, len(values), 3)]), clamp)
        for clamp in [True, False]]
    # The source ratio rounds once before CMS. Composition takes that stored
    # F32 (or independently checked actual canonical F32), not an ideal real.
    fallback = []
    for keep in [False, True]:
        by_background = []
        for background in range(3):
            bg = [F(v/255.0) for v in [19, 43, 71]]
            if background == 1:
                bg = [decode(v) for v in bg]
            out = []
            for i, value in enumerate(values):
                a = pixels[i//3][-1]
                alpha = F(a/float(maximum))
                if background and a < maximum and (a or not keep):
                    value = encode(alpha*decode(F(value)) +
                                   (1-alpha)*bg[i%3])
                out.append(ref.nearest_rational(F(value)))
            by_background.append(out)
        fallback.append(by_background)
    managed = []
    for bg in [False, True]:
        by_engine = []
        for canon in canonical:
            by_engine.append([float(F(pixels[i//3][-1], maximum)*F(v) +
                (1-F(pixels[i//3][-1], maximum))*decode(
                    F([19, 43, 71][i%3], 255)))
                if bg and pixels[i//3][-1] < maximum else v
                for i, v in enumerate(canon)])
        managed.append(by_engine)
    byte = [max(0, min(255, int(v*255 + F(1, 2)))) for v in source]
    return dict(name=name, photo=photo, associated=int(association == 1),
        opaque=int(all(row[-1] == maximum for row in pixels)),
        bits=bits, count=len(pixels), stored=sum(pixels, []),
        alpha=[row[-1] for row in pixels], source=words, canonical=canonical,
        fallback=fallback, managed=managed, bytes=byte)


def c_values(values, fmt, indent=8):
    if isinstance(values[0], list):
        return ',\n'.join(' '*indent + '{\n' + c_values(v, fmt, indent+4) +
                           '\n' + ' '*indent + '}' for v in values)
    return ',\n'.join(' '*indent + fmt % v for v in values)


def integer16_samples(photo, association, kind):
    """Include one-rounding and nonzero-alpha witnesses at their source."""
    alpha = [0, 1, 32768, 65535]
    if kind == 'edge':
        alpha = [0, 1, 128, 129, 256, 257, 32767, 32768, 65534, 65535]
        colors = [[a//2, a//4, a//8] for a in alpha]
        colors[1] = [2, 1, 0]
    else:
        colors = [[0, 0, 0], [1, 0, 0], [16449, 8192, 4096],
                  [32897, 16449, 8225]]
    if association == 2:
        colors = [[32897, 16449, 8225]] * len(alpha)
    if kind == 'opaque':
        alpha = [65535] * len(alpha)
    if photo != 2:
        colors = [row[:1] for row in colors]
    if photo == 0:
        colors = [[65535-v for v in row] for row in colors]
    return [row + [a] for row, a in zip(colors, alpha)]


def add_integer16(outputs, records, cases, p3, gray):
    # Preserve the original twenty byte-case indexes. Normal, boundary and
    # opaque UINT16 cases occupy six indexes each, with explicit bit/count
    # metadata; test observers do not infer storage from a fixture name.
    layouts = [(4, 4, '>'), (2, 1, '<'), (2, 4, '<'),
               (3, 1, '>'), (3, 4, '>'), (4, 1, '<')]
    for kind in ['normal', 'edge', 'opaque']:
        for i, (name, photo, association) in enumerate(
                (name, photo, a) for name, photo in
                [('rgb', 2), ('black', 1), ('white', 0)] for a in [1, 2]):
            name = f'{name}-16-{kind}-a{association}'
            profile = p3 if photo == 2 else gray
            pixels = integer16_samples(photo, association, kind)
            cases.append(oracle(name, photo, association, pixels, profile, 16))
            variants = [('icc', profile, 1, None, 1, '<', association)]
            if kind != 'edge':
                variants += [('none', None, 1, None, 1, '<', association),
                             ('invalid', b'not-an-icc', 1, None, 1, '<',
                              association),
                             ('mismatch', gray if photo == 2 else p3,
                              1, None, 1, '<', association)]
            if kind == 'normal':
                o, s, e = layouts[i]
                variants += [('o1-s1', profile, 1, 2, 1, '<', association),
                             ('layout', profile, o, 2, s, e, association)]
            if kind == 'opaque' and association == 2:
                variants.append(('noalpha', profile, 1, None, 1, '<', 0))
            for suffix, icc, orientation, width, sample, endian, alpha in variants:
                filename = f'tiff-alpha-{name}-{suffix}.tif'
                rows = pixels if alpha else [row[:-1] for row in pixels]
                blob = write_tiff(rows, photo, icc, orientation, width,
                                  sample, alpha, 16, endian)
                outputs[filename] = blob
                records.append(dict(file=filename,
                                    sha256=hashlib.sha256(blob).hexdigest()))
    # These are the established four-pixel IEEEFP32-alpha rejection controls.
    # The integer path must not silently add a new float-alpha decoder.
    for name, photo in [('rgb', 2), ('black', 1), ('white', 0)]:
        for association in [1, 2]:
            colors = ([[0, 0, 0], [1/255, 0, 0], [0.25, 0.125, 0.0625],
                       [0.5, 0.25, 0.125]] if association == 1 else
                      [[0.5, 0.25, 0.125]] * 4)
            if photo != 2:
                colors = [row[:1] for row in colors]
            if photo == 0:
                colors = [[1-v for v in row] for row in colors]
            pixels = [row+[a] for row, a in zip(colors, [0, 1/255, 0.5, 1])]
            filename = f'tiff-alpha-{name}-32-a{association}.tif'
            blob = write_tiff(pixels, photo, p3 if photo == 2 else gray,
                              sample=3, alpha=association, bits=32)
            outputs[filename] = blob
            records.append(dict(file=filename,
                                sha256=hashlib.sha256(blob).hexdigest()))


def build(p3, gray):
    outputs, records, cases = {}, [], []
    specs = [(name + '-a' + str(a), photo, a, 'normal')
             for name, photo in [('rgb', 2), ('black', 1), ('white', 0)]
             for a in [1, 2]]
    specs += [('rgb-extended-a'+str(a), 2, a, 'extended') for a in [1, 2]]
    specs += [(name + '-'+kind+'-a'+str(a), photo, a, kind)
              for kind in ['opaque', 'all255']
              for name, photo in [('rgb', 2), ('black', 1), ('white', 0)]
              for a in [1, 2]]
    for name, photo, association, kind in specs:
        profile = p3 if photo == 2 else gray
        pixels = samples(photo, association, kind)
        cases.append(oracle(name, photo, association, pixels, profile))
        variants = [('icc', profile, 1, None, 1)]
        if kind in ['normal', 'all255']:
            variants += [('none', None, 1, None, 1),
                         ('invalid', b'not-an-icc', 1, None, 1),
                         ('mismatch', gray if photo == 2 else p3, 1, None, 1)]
        if kind == 'normal' and association == 1:
            variants += [(f'o{o}-s{s}', profile, o, 2, s)
                         for o in range(1, 5) for s in [1, 4]]
        if kind == 'normal' and association == 2:
            variants.append(('o1-s1', profile, 1, 2, 1))
        for suffix, icc, orientation, width, sample in variants:
            filename = f'tiff-alpha-{name}-{suffix}.tif'
            blob = write_tiff(pixels, photo, icc, orientation, width, sample,
                              association)
            outputs[filename] = blob
            records.append(dict(file=filename,
                                sha256=hashlib.sha256(blob).hexdigest()))
    add_integer16(outputs, records, cases, p3, gray)
    header = """/* Independent original-alpha source and stored-ICC oracle. */
#ifndef SIXEL_TIFF_ALPHA_VALUES_H
#define SIXEL_TIFF_ALPHA_VALUES_H
#include <stdint.h>
static struct {
    char const *name;
    int photo;
    int associated;
    int opaque;
    int bits;
    int count;
    uint16_t stored[40];
    uint16_t alpha[10];
    uint32_t source[30];
    unsigned char bytes[30];
    double managed[2][2][30];
    double canonical[2][30];
    uint32_t fallback[2][3][30];
} const ta_cases[] = {
"""
    for case in cases:
        header += '    { "%s", %d, %d, %d, %d, %d, {\n' % (
            case['name'], case['photo'], case['associated'], case['opaque'],
            case['bits'], case['count'])
        for field, fmt in [('stored', '%d'), ('alpha', '%d'),
                           ('source', '0x%08xu'), ('bytes', '%d'),
                           ('managed', '%.17g'), ('canonical', '%.17g'),
                           ('fallback', '0x%08xu')]:
            header += c_values(case[field], fmt)
            header += '\n    }' + (', {\n' if field != 'fallback' else ' },\n')
    header += '};\n#endif\n'
    outputs['tiff-alpha.h'] = header.encode()
    outputs['tiff-alpha.json'] = (json.dumps(dict(
        generator='tools/generate_tiff_alpha_fixtures.py',
        oracle='Original integer ratios with one nearest-even binary32 '
               'rounding; independent raw ICC tags, rational matrices and '
               'Decimal power; stored-F32 composition model, no CMM.',
        native_domain='Builtin keeps its existing device-domain clipping; '
                      'native engines keep their existing profile behavior. '
                      'The adapter does not clip original associated ratios.',
        cases=cases, fixtures=records), indent=2)+'\n').encode()
    return outputs
