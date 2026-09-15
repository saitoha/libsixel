#!/usr/bin/env python3
"""Existing independent device specimens and LCMS alias-control intervals."""

from decimal import Decimal as D, localcontext
import hashlib
import json

from generate_lcms_input_tests import array, srgb
from generate_tiff_fixtures import f32


def build(png_record, gray_record, cmyk_record, profiles):
    with localcontext() as context:
        context.prec = 90
        return build_values(json.loads(png_record), json.loads(gray_record),
                            json.loads(cmyk_record), profiles)


def build_values(png, gray, cmyk, profiles):
    rgb = next(p['samples'] for p in png['pngs']
               if p['file'] == 'png-p3-8-rgb.png')
    rgba = next(p['samples'] for p in png['pngs']
                if p['file'] == 'png-p3-8-rgba.png')
    assert [p[:3] for p in rgba] == rgb
    rgb_linear = png['expected']['icc_linear'][0]
    gray_float = [[f32(D(v) / 65535)] for v in gray['source']]
    gray_linear = [[(D(v) / 65535) ** 2] * 3 for v in gray['source']]
    gray_byte = [[v] for v in range(256)]
    byte_linear = [[(D(v) / 255) ** 2] * 3 for v in range(256)]
    specs = [(1, gray_byte, byte_linear, 'gray-gamma2.icc', 'gray'),
             (2, rgb, rgb_linear, 'display-p3.icc', 'rgb'),
             (3, rgba, rgb_linear, 'display-p3.icc', 'rgb'),
             (5, cmyk['source'], cmyk['linear'], 'cmyk.icc', 'cmyk'),
             (6, cmyk['source16'], cmyk['linear16'], 'cmyk.icc', 'cmyk'),
             (9, gray_float, gray_linear, 'gray-gamma2.icc', 'gray')]
    budget = D(64) / 2**24
    assert D(str(gray['error_contract']['float_relative_budget'])) == budget
    record = []
    header = '/* Existing independent specimens; propagated LCMS budgets. */\n'
    for index, (fmt, samples, linear, profile, family) in enumerate(specs):
        expected, intervals = [], []
        for i, triplet in enumerate(linear):
            triplet = [v if isinstance(v, D) else D(str(v)) for v in triplet]
            if family == 'gray':
                # Preserve the existing gray relative allowance, including
                # first float input normalization. Black has no floor.
                radius = [max(map(abs, triplet)) * budget] * 3
            else:
                # The existing integer-RGB and integer-CMYK C tests use
                # 5e-7 absolute linear rounding allowance. Interpolated CMYK
                # samples additionally propagate two u1.15 PCS bins.
                radius = [D('.0000005')] * 3
                if family == 'cmyk' and i in [5, 6]:
                    radius = [v + D(str(q)) for v, q in zip(radius,
                        cmyk['lut_precision_budget']['linear_rgb'])]
            lo = [v - t for v, t in zip(triplet, radius)]
            hi = [v + t for v, t in zip(triplet, radius)]
            encoded = [srgb(v) for v in triplet]
            # Monotone native sRGB inverse transports the existing linear
            # interval. Reserve the existing float-operation triplet budget
            # for the added transfer/storage, without fitting observations.
            gamma_margin = max(map(abs, encoded)) * budget
            gamma_bounds = [[srgb(v) - gamma_margin for v in lo],
                            [srgb(v) + gamma_margin for v in hi]]
            expected.append([triplet, encoded])
            intervals.append([[lo, hi], gamma_bounds])
        padded = [row + [0] * (4 - len(row)) for row in samples]
        header += array(f'ea_source_{index}', 'float', padded)
        header += array(f'ea_bounds_{index}', 'double', intervals)
        record.append(dict(format=fmt, profile=profile, family=family,
            source=samples, expected=expected, bounds=intervals))
    header += ('static struct {\n    int format;\n    size_t count;\n'
               '    float const (*source)[4];\n'
               '    double const (*bounds)[2][2][3];\n'
               '} const ea_cases[] = {\n')
    for i, (fmt, samples, _, _, _) in enumerate(specs):
        header += (f'    {{ {fmt}, {len(samples)}, ea_source_{i}, '
                   f'ea_bounds_{i} }},\n')
    header += '};\n'
    data = dict(precision_decimal=90, cases=record,
        profiles={name: hashlib.sha256(blob).hexdigest()
                  for name, blob in profiles.items()},
        source_records=['png-samples.json', 'gray-samples.json', 'cmyk.json'],
        linear_allowance='RGB/RGBA: existing 0025 absolute 5e-7; CMYK8/16: existing 0033 absolute 5e-7 plus two PCS bins for samples 5/6; gray: existing 0045 relative 64*2^-24 with exact black.',
        gamma_allowance='Map linear interval through native monotone sRGB inverse (linear negative extension), plus encoded triplet magnitude * 64*2^-24 for transfer/storage; no observed-value fitting.',
        gray_byte_model='All 256 exact bytes: (v/255)^2, as existing builtin-device independent specimen; LCMS gray float allowance applies.',
        gray_float_model='Existing 18 source words normalized to binary32; independent ideal (v/65535)^2 retains existing input-rounding allowance.')
    return {'lcms-expanding-alias.h': header.encode(),
            'lcms-expanding-alias.json': (json.dumps(data, indent=2,
                                                   default=str) + '\n').encode()}
