#!/usr/bin/env python3
"""Independent selected decoded-YCbCr RGB8 and stored RGB ICC values."""
from fractions import Fraction as F
import hashlib
import json
import struct

import tiff_managed_reference as reference
from generate_cli_color_fixtures import embedded_profile, wire_color


def build(inputs):
    item = next(x for x in json.loads(inputs['tiff-legacy.json'])['cases']
                if x['name'] == 'ycbcr8-111')
    blob = inputs['tiff-legacy-ycbcr8-111.tif']
    profile = embedded_profile(blob)
    assert profile == inputs['display-p3.icc']
    decoded = []
    for y, cb, cr in item['samples']:
        # TIFF's default coefficients and full-range ReferenceBlackWhite.
        # These selected values are away from byte ties; the test separately
        # verifies the actual libtiff raster before it asserts CMS inputs.
        r = F(y) + 2 * (1-F('0.299')) * (cr-128)
        b = F(y) + 2 * (1-F('0.114')) * (cb-128)
        g = (F(y) - F('0.299')*r - F('0.114')*b) / F('0.587')
        decoded.append([min(255, max(0, int(v+F(1,2)))) for v in [r,g,b]])
    words = [[reference.nearest_rational(F(v,255)) for v in row]
             for row in decoded]
    floats = [[struct.unpack('>f',struct.pack('>I',v))[0] for v in row]
              for row in words]
    linear = reference.expected(dict(icc=profile,triples=floats))
    header = '/* Selected decoded RGB8 boundary, then independent ICC math. */\n'
    for ctype,name,values in [('unsigned char','yc_decoded',sum(decoded,[])),
                              ('uint32_t','yc_source',sum(words,[])),
                              ('double','yc_linear_values',linear)]:
        header += 'static '+ctype+' const '+name+'[18] = {\n'
        for i in range(0,18,3):
            header += '    '+', '.join(map(str,values[i:i+3]))+',\n'
        header += '};\n'
    record=dict(generator='tools/generate_tiff_ycbcr_tests.py',
        source=item['samples'],decoded_rgb8=decoded,input_binary32=words,
        linear=linear,wire=[wire_color(linear[i:i+3]) for i in range(0,18,3)],
        profile_sha256=hashlib.sha256(profile).hexdigest(),
        fixture_sha256=hashlib.sha256(blob).hexdigest(),
        boundary='Selected opaque YCbCr8 decoded RGB8; does not preserve original component precision.',
        oracle='Exact default coefficient math for selected RGB8 samples; rational binary32 rounding; independent stored RGB ICC curves and matrices.',
        builtin_budget='64 u32 times maximum absolute output component per triplet; native controls qualify adapter routing against the same direct CMM input.')
    return {'tiff-ycbcr.h':header.encode(),
            'tiff-ycbcr.json':(json.dumps(record,indent=2)+'\n').encode()}
