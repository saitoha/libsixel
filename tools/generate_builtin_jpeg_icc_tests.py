#!/usr/bin/env python3
"""Analytic source-domain JPEG fixtures; no JPEG codec or CMM is an oracle."""
from decimal import Decimal as D, localcontext
from pathlib import Path
import argparse
import hashlib
import json
import struct

from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, destination_matrix, inverse

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/data/color-numeric'


def marker(code, payload):
    return bytes((255, code)) + struct.pack('>H', len(payload) + 2) + payload


class Bits:
    def __init__(self):
        self.bits = ''

    def difference(self, value):
        category = abs(value).bit_length()
        self.bits += f'{category:05b}'
        if category:
            self.bits += f'{value if value >= 0 else (1 << category) - 1 + value:0{category}b}'

    def end(self):
        padded = self.bits + '1' * (-len(self.bits) % 8)
        data = bytes(int(padded[i:i + 8], 2) for i in range(0, len(padded), 8))
        return data.replace(b'\xff', b'\xff\0')


def jpeg(depth, model, samples, profile, progressive=False):
    """Q=1/DC-only DCT, or predictor-1 lossless. No quantization ambiguity."""
    lossless = depth == 16
    width, height = (5, 3) if lossless else (17, 9)
    data = b'\xff\xd8'
    if model != 'plain':
        data += marker(0xee, b'Adobe' + struct.pack('>3HB', 100, 0, 0, 2 if model == 'ycck' else 0))
    if profile is not None:
        data += marker(0xe2, b'ICC_PROFILE\0\1\1' + profile)
    sof = 0xc3 if lossless else 0xc2 if progressive else 0xc0 if depth == 8 else 0xc1
    data += marker(sof, struct.pack('>BHHB', depth, height, width, 4)
                   + b''.join(bytes((i + 1, 0x11, 0)) for i in range(4)))
    if not lossless:
        data += marker(0xdb, bytes((0,)) + bytes((1,)) * 64)
    data += marker(0xc4, bytes((0,)) + bytes((0, 0, 0, 0, 16)) + bytes(11) + bytes(range(16)))
    if not lossless:
        data += marker(0xc4, bytes((16, 1)) + bytes(15) + bytes((0,)))
    data += marker(0xda, bytes((4,)) + b''.join(bytes((i + 1, 0)) for i in range(4))
                   + bytes((1 if lossless else 0, 0 if lossless or progressive else 63, 0)))
    bits = Bits()
    previous = [0] * 4
    for i, sample in enumerate(samples):
        for c, value in enumerate(sample):
            if lossless:
                predictor = (1 << (depth - 1)) if i == 0 else samples[i - (5 if i % 5 == 0 else 1)][c]
                difference = value - predictor
            else:
                dc = 8 * (value - (1 << (depth - 1)))
                difference = dc - previous[c]
                previous[c] = dc
            bits.difference(difference)
            if not lossless and not progressive:
                bits.bits += '0'
    data += bits.end()
    if progressive:
        for c in range(4):
            data += marker(0xda, bytes((1, c + 1, 0, 1, 63, 0)))
            bits = Bits()
            bits.bits = '0' * len(samples)
            data += bits.end()
    return data + b'\xff\xd9'


def stored(name, content):
    # The aggregate color fixture generator supplies the same header guard.
    if name.endswith('.h'):
        guard = 'COLOR_FIXTURE_' + name[:-2].upper().replace('-', '_')
        return f'#ifndef {guard}\n#define {guard}\n'.encode() + content + b'#endif\n'
    return content


def build(profile=None, record_bytes=None, gray_profile=None):
    profile = profile if profile is not None else (OUT / 'cmyk.icc').read_bytes()
    record = json.loads(record_bytes if record_bytes is not None else (OUT / 'cmyk.json').read_bytes())
    gray_profile = gray_profile if gray_profile is not None else (OUT / 'gray-gamma2.icc').read_bytes()
    missing = bytearray(profile)
    # Strict relative-colorimetric apply must fail after transform creation.
    index = missing.index(b'A2B1', 132)
    missing[index:index + 4] = b'A2B2'
    outputs, entries, expected, fallback = {}, [], [], []
    with localcontext() as ctx:
        ctx.prec = 80
        matrix = inverse(destination_matrix())
        for depth, model, progressive in [(d, m, False) for d in (8, 12, 16)
                for m in ('plain', 'adobe', 'ycck')] + [(8, 'adobe', True)]:
            maximum = (1 << depth) - 1
            if model == 'ycck':
                center = 1 << (depth - 1)
                samples = [(maximum // 8, center, center, maximum),
                           (maximum // 2, center + maximum // 8, center - maximum // 9, maximum // 2),
                           (maximum, center, center, 0),
                           (maximum // 4, center - maximum // 9, center + maximum // 8, maximum - 1),
                           (0, center, center, maximum),
                           (maximum * 3 // 4, center, center, maximum // 4)]
            else:
                samples = [(0, maximum, maximum, 0), (maximum, 0, maximum, 0),
                           (maximum, maximum, 0, 0),
                           (maximum // 4, maximum // 2, maximum * 3 // 4, maximum // 8),
                           (0, 0, 0, 0), (maximum, maximum, maximum, maximum)]
            if depth == 16:
                # Adjacent values in every source channel must survive decode.
                samples = [tuple(8192 + c * 7000 + i for c in range(4)) for i in range(15)]
            name = f'builtin-jpeg-{model}{depth}' + ('-progressive' if progressive else '')
            output = []
            normalized = []
            unmanaged = []
            for sample in samples:
                unit = [D(v) / maximum for v in sample]
                if model == 'ycck':
                    y, cb, cr, k = map(D, sample)
                    center = D(1 << (depth - 1))
                    unit = [min(D(1), max(D(0), v / maximum)) for v in (
                        y + D('1.402') * (cr - center),
                        y - D('.34414') * (cb - center) - D('.71414') * (cr - center),
                        y + D('1.772') * (cb - center))] + [1 - k / maximum]
                elif model == 'adobe':
                    unit = [1 - v for v in unit]
                xyz = [(D(record['base_u1_15'][j]) - sum(unit[c] * record['weights_u1_15'][c][j]
                        for c in range(4))) / 32768 for j in range(3)]
                output.append(apply(matrix, xyz))
                normalized.append(unit)
                unmanaged.append([(1 - v) * (1 - unit[3]) for v in unit[:3]])
            entries.append(dict(name=name, depth=depth, model=model, progressive=progressive,
                                samples=samples, expected=output, device=normalized))
            expected.append(output + [[D(0)] * 3] * (15 - len(output)))
            if depth == 8 and model == 'adobe' and not progressive:
                fallback = unmanaged
            outputs[name + '.jpg'] = jpeg(depth, model, samples, profile, progressive)
            if depth == 8 and model == 'adobe' and not progressive:
                for suffix, icc in [('absent', None), ('rejected', b'broken ICC'),
                                    ('failure', bytes(missing)),
                                    ('mismatch', gray_profile)]:
                    outputs[name + '-' + suffix + '.jpg'] = jpeg(depth, model, samples, icc)
        header = '/* Generated analytic constant-block/lossless source values. */\n'
        header += array('bj_expected', 'double', expected)
        header += array('bj_fallback', 'double', fallback)
        header += array('bj_pcs_bound', 'double', record['lut_precision_budget']['linear_rgb'])
        outputs['builtin-jpeg-icc.h'] = header.encode()
    manifest = dict(generator='tools/generate_builtin_jpeg_icc_tests.py',
        oracle='80-digit Decimal affine ICC PCS and independently solved D50 sRGB matrix; no CMM',
        profile_sha256=hashlib.sha256(profile).hexdigest(), entries=entries,
        sha256={name: hashlib.sha256(stored(name, data)).hexdigest() for name, data in outputs.items()})
    outputs['builtin-jpeg-icc.json'] = (json.dumps(manifest, indent=2, default=str) + '\n').encode()
    return outputs


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for name, content in build().items():
        path = OUT / name
        content = stored(name, content)
        if args.check:
            assert path.read_bytes() == content, f'stale {path}'
        else:
            path.write_bytes(content)
