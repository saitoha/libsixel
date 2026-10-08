#!/usr/bin/env python3
"""Literal TIFF Lab/ink sources and independent Decimal color equations."""
from decimal import Decimal as D, localcontext
from pathlib import Path
import argparse
import hashlib
import json
import struct

from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, f32, destination_matrix, inverse
from generate_tiff_icc_tests import gamma, lab_xyz, read_tiff
from generate_tiff_lab_layout_tests import directory
from generate_tiff_lab_linear_tests import linear_matrices, linear_bounds
from generate_builtin_jpeg_icc_tests import stored

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/data/color-numeric'


def fixture(item, profile):
    """Write TIFF words and row-local PackBits, without an image codec."""
    bits, photo, orientation, planar, tiled, packed, alpha, big = item['spec']
    endian = '>' if big else '<'
    raw = item['raw']
    channels = len(raw[0])
    tags = {256: (4, [5]), 257: (4, [3]), 258: (3, [bits] * channels),
            259: (3, [32773 if packed else 1]), 262: (3, [photo]),
            274: (3, [orientation]), 277: (3, [channels]),
            284: (3, [2 if planar else 1]), 339: (3, [1] * channels)}
    if profile is not None:
        tags[34675] = (7, profile)
    if alpha:
        tags[338] = (3, [alpha])
    if photo == 5:
        tags[332] = (3, [1])
        tags[334] = (3, [4])
    if photo == 10:
        tags[433] = (10, [v for n in [5, 95, -40, 60, -80, 20] for v in (n, 1)])
    width = height = 16 if tiled else 0
    width = width or 5
    height = height or 3
    parts = []
    for plane in range(channels if planar else 1):
        rows = []
        for y in range(height):
            samples = []
            for x in range(width):
                pixel = raw[y * 5 + x] if x < 5 and y < 3 else [0] * channels
                samples.extend([pixel[plane]] if planar else pixel)
            blob = struct.pack(endian + ('H' if bits == 16 else 'B') * len(samples), *samples)
            if packed:
                blob = b''.join(bytes([len(blob[i:i+128]) - 1]) + blob[i:i+128]
                                for i in range(0, len(blob), 128))
            rows.append(blob)
        parts.append(b''.join(rows))
    offset_tag, size_tag = (324, 325) if tiled else (273, 279)
    if tiled:
        tags.update({322: (4, [16]), 323: (4, [16])})
    else:
        tags[278] = (4, [3])
    tags[offset_tag] = (4, [0] * len(parts))
    tags[size_tag] = (4, list(map(len, parts)))
    offset = len(directory(tags, endian))
    for i, part in enumerate(parts):
        tags[offset_tag][1][i] = offset
        offset += len(part)
    return directory(tags, endian) + b''.join(parts)


def build(linear_profile=None, cmyk_profile=None, cmyk_record=None):
    linear_profile = linear_profile or (OUT / 'cli-rgb.icc').read_bytes()
    cmyk_profile = cmyk_profile or (OUT / 'cmyk.icc').read_bytes()
    cmyk_record = json.loads(cmyk_record or (OUT / 'cmyk.json').read_bytes())
    with localcontext() as ctx:
        ctx.prec = 90
        return values(linear_profile, cmyk_profile, cmyk_record)


def values(linear_profile, cmyk_profile, record):
    lab_profile = read_tiff('lab_high_precision_icc.tiff', True)['profile']
    matrices = linear_matrices(linear_profile)
    matrix = inverse(destination_matrix())
    d65 = [[D(v) for v in row] for row in
           [['3.2404542', '-1.5371385', '-.4985314'],
            ['-.9692660', '1.8760108', '.0415560'],
            ['.0556434', '-.2040259', '1.0572252']]]
    # bits, photometric, orientation, separate, tiled, PackBits, alpha, BE
    specs = [(8,8,1,0,0,0,0,0), (8,8,2,1,0,1,0,1),
             (8,8,6,0,1,0,0,0), (16,8,8,1,1,1,0,1),
             (8,9,3,1,0,1,0,0), (8,10,7,1,1,1,0,1),
             (8,5,1,0,0,0,0,0), (8,5,4,1,0,1,0,1),
             (8,5,6,0,1,0,0,0), (16,5,8,1,1,1,0,1),
             (8,8,1,1,0,1,2,0), (8,5,6,0,1,0,2,0),
             (16,5,8,1,1,1,1,1), (16,8,6,0,0,0,0,0),
             (8,5,1,0,0,0,1,0), (8,8,8,1,1,1,1,1)]
    header = '/* Literal source words; independently solved CMS bounds. */\n'
    entries, manifest = [], []
    for index, spec in enumerate(specs):
        bits, photo, orientation, planar, tiled, packed, alpha, big = spec
        maximum = (1 << bits) - 1
        channels = 4 if photo == 5 else 3
        raw, ordered, coverage = [], [None] * 15, [0] * 15
        for i in range(15):
            a = ([0, 1, maximum//2, maximum-1, maximum][i % 5]
                 if alpha and index != 14 else maximum)
            if photo == 5:
                pixel = [(maximum * (i*3+c*7+1) // 61 + i) % (maximum+1) for c in range(4)]
                if i < 6:
                    pixel = [[0,maximum,maximum,0], [maximum,0,maximum,0],
                             [maximum,maximum,0,0], [0,0,0,0],
                             [maximum]*4, [1,2,3,4]][i]
                if alpha == 1:
                    pixel = [v * a // maximum for v in pixel]
                normalized = [f32(D(v) / (a if alpha == 1 else maximum)) if a or alpha != 1 else 0.0 for v in pixel]
            else:
                lightness = maximum * (i+1) // 17
                ab = [(i*11 % 81)-40, (i*7 % 91)-45]
                pixel = [lightness] + ([v & 255 for v in ab] if photo == 8 and bits == 8 else
                        [(v*256 + i) & 65535 for v in ab] if photo == 8 else
                        [v+128 for v in ab])
                physical = [D(lightness) * 100 / maximum] + list(map(D, ab))
                if photo == 8 and bits == 16:
                    physical[1:] = [D(v) + D(i)/256 for v in ab]
                elif photo == 10:
                    ranges = [5,95,-40,60,-80,20]
                    physical = [D(ranges[2*c]) + D(pixel[c])/255*(ranges[2*c+1]-ranges[2*c]) for c in range(3)]
                if alpha == 1:
                    # Store associated signed physical chroma, then derive
                    # the expected straight coordinates from those bytes.
                    signed = [int(D(v) * a / maximum) for v in ab]
                    pixel = [lightness * a // maximum] + [v & 255 for v in signed]
                    physical = ([D(pixel[0])*100/a] +
                                [D(v)*maximum/a for v in signed]) if a else [D(0)]*3
                normalized = [f32(v / (100 if c == 0 else 128)) for c, v in enumerate(physical)]
            raw.append(pixel + ([a] if alpha else []))
            x, y = i % 5, i // 5
            dx, dy = [(x,y),(4-x,y),(4-x,2-y),(x,2-y),(y,x),(2-y,x),(2-y,4-x),(y,4-x)][orientation-1]
            slot = dy * (3 if orientation >= 5 else 5) + dx
            ordered[slot] = normalized
            coverage[slot] = a
        limits, fallback = [], []
        for row in ordered:
            if photo == 5:
                source = list(map(D.from_float, row))
                xyz = [(D(record['base_u1_15'][j]) - sum(source[c]*record['weights_u1_15'][c][j] for c in range(4))) / 32768 for j in range(3)]
                ideal = apply(matrix, xyz)
                scale = max(map(abs, ideal)) * D(64) / 2**24
                bounds = [[[v-scale-(D(str(record['lut_precision_budget']['linear_rgb'][c])) if engine else 0) for c,v in enumerate(ideal)],
                           [v+scale+(D(str(record['lut_precision_budget']['linear_rgb'][c])) if engine else 0) for c,v in enumerate(ideal)]] for engine in range(3)]
                if bits == 8 and (alpha != 1 or index == 14):
                    words = [int(v*255+D('.5')) for v in source]
                    fb = [D((255-words[c])*(255-words[3])//255)/255 for c in range(3)]
                else:
                    fb = [(1-v)*(1-source[3]) for v in source[:3]]
            else:
                bounds = linear_bounds(row, matrices)
                xyz = lab_xyz([D.from_float(v)*(100 if c == 0 else 128) for c,v in enumerate(row)])
                if bits == 16:
                    xyz = [v*w/p for v,w,p in zip(xyz,map(D,['.95047','1','1.08883']),map(D,['.9642','1','.8249']))]
                    fb = [gamma(v) for v in apply(d65,xyz)]
                else:
                    fb = apply(matrix,xyz)
            limits.append(bounds)
            fallback.append(fb)
        item = dict(spec=spec,raw=raw,source=ordered,alpha=coverage,bounds=limits,fallback=fallback)
        profile = cmyk_profile if photo == 5 else lab_profile
        for suffix, icc in [('',profile),('-absent',None),('-invalid',b'broken ICC'),('-mismatch',lab_profile if photo == 5 else cmyk_profile)]:
            blob = fixture(item,icc)
            symbol = f'td_data_{index}' + suffix.replace('-','_')
            header += f'static unsigned char const {symbol}[] = {{\n'
            for start in range(0,len(blob),12):
                header += '    ' + ', '.join(f'0x{b:02x}' for b in blob[start:start+12]) + ',\n'
            header += '};\n'
        header += array(f'td_source_{index}','float',sum(ordered,[]))
        header += array(f'td_alpha_{index}','unsigned int',coverage)
        header += array(f'td_bounds_{index}','double',limits)
        header += array(f'td_fallback_{index}','double',fallback)
        blobs = ', '.join(f'td_data_{index}{s}' for s in ('','_absent','_invalid','_mismatch'))
        sizes = ', '.join(f'sizeof(td_data_{index}{s})' for s in ('','_absent','_invalid','_mismatch'))
        entries.append(f'    {{{{{blobs}}}, {{{sizes}}}, {bits}, {photo}, {orientation}, {alpha},\n        td_source_{index}, td_alpha_{index}, td_bounds_{index}, td_fallback_{index}}},\n')
        manifest.append(item)
    header += ('static struct {\n    unsigned char const *bytes[4];\n    size_t sizes[4];\n'
               '    int bits, photo, orientation, alpha;\n    float const *source;\n'
               '    unsigned int const *coverage;\n    double const (*bounds)[3][2][3];\n'
               '    double const (*fallback)[3];\n} const td_cases[] = {\n' + ''.join(entries) + '};\n')
    outputs = {'tiff-device.h':header.encode()}
    outputs['tiff-device.json'] = (json.dumps(dict(generator='tools/generate_tiff_device_tests.py',
        oracle='Literal TIFF signed/offset Lab and positive ink ratios; 90-digit Decimal ICC PCS equations, no live CMM; inherited Lab/CMYK numerical budgets.',
        cases=manifest,sha256={k:hashlib.sha256(stored(k,v)).hexdigest() for k,v in outputs.items()}),indent=2,default=str)+'\n').encode()
    return outputs


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args = parser.parse_args()
    for name, data in build().items():
        path = OUT / name
        data = stored(name,data)
        if args.check:
            assert path.read_bytes() == data, f'stale {path}'
        else:
            path.write_bytes(data)
