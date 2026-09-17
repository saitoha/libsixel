#!/usr/bin/env python3
"""Prescribed LogLuv words, direct TIFF storage and independent frame values."""
from decimal import Decimal as D, ROUND_FLOOR, localcontext
import hashlib
import json
import math
import struct

from generate_lcms_input_tests import array
from generate_tiff_fixtures import apply, inverse, f32

# Selected rows of Greg Ward's published SGILOG24 codebook (libtiff uvcode.h).
# Each tuple is (row index, u start, number of cells, cumulative cell index).
# The codec is not invoked to generate either storage or expected values.
UV_ROWS = [(130, '.009016', 170, 12209),
           (128, '.009991', 168, 11873),
           (161, '.016994', 31, 16237),
           (141, '.002389', 170, 14121),
           (156, '.003246', 69, 15964)]


def black_lightness_model():
    """Bound cancellation at zero without relaxing other sample intervals."""
    with localcontext() as context:
        context.prec = 110
        u, u32 = D(2)**-53, D(2)**-24
        gamma = lambda n: n*u/(1-n*u)
        f0 = D(16)/116
        ef = gamma(1)*abs(f0)
        # Cover the rounded f0, multiply and cancellation, with or without FMA.
        estar = 116*ef + gamma(2)*(116*(abs(f0)+ef)+16)
        el = estar/100*(1+gamma(1))*(1+u32) + D(2)**-150
        c1, c2 = D.from_float(325.22), D.from_float(.0036)
        zmax = abs(c2)*estar*(1+gamma(1))
        # The finite log1p probe checks this existing 64u32 tolerance on the
        # running libm. This is not a claim about ISO C or fast-math accuracy.
        ed = abs(c1)*(-(1-zmax).ln())*(1+64*u32)*(1+gamma(1))**2/100
        ed = ed*(1+u32) + D(2)**-150

        def outward(value, upper):
            rounded = float(value)
            if (D.from_float(rounded) < value if upper else
                    D.from_float(rounded) > value):
                rounded = math.nextafter(rounded, math.inf if upper else -math.inf)
            return rounded

        residual = 116*D.from_float(float(f0))-16
        zres = float(c2*residual)
        probes = []
        for z in (0., -0., zres, -zres, float(zmax), -float(zmax)):
            exact = (1+D.from_float(z)).ln()
            budget = abs(exact)*64*u32
            probes.append(dict(bits=struct.unpack('>Q', struct.pack('>d', z))[0],
                input=z.hex(), exact=str(exact),
                lo=outward(exact-budget, False), hi=outward(exact+budget, True)))
        bounds = [outward(el, True), outward(ed, True)]
        record = dict(model='Binary64 RN with or without FMA; binary32 RN publication',
            libm_model='Finite log1p probes within 64u32 relative error; no fast-math',
            gamma_n='n*u/(1-n*u)', u=str(u), u32=str(u32),
            E_star=str(estar), E_L32=str(el), zmax=str(zmax), E_D32=str(ed),
            bounds=bounds, fma_residual=str(residual), probes=probes)
        header = ('/* Generated cancellation bounds and finite libm probes. */\n'
                  + 'static double const color_black_l_bounds[2] = {\n'
                  + ''.join('    %.17g,\n' % bound for bound in bounds)
                  + '};\n'
                  + 'static struct color_black_log_probe {\n'
                    '    uint64_t bits;\n    double lo;\n    double hi;\n'
                    '} const color_black_log_probes[] = {\n')
        for p in probes:
            header += '    { UINT64_C(0x%016x),\n      %.17g, %.17g },\n' % (
                p['bits'], p['lo'], p['hi'])
        header += '};\n'
        return bounds, record, header


def floor(value):
    return int(value.to_integral_value(rounding=ROUND_FLOOR))


def binary32(value):
    return D.from_float(f32(D(value)))


def words_and_centers(samples, bits):
    words, centers = [], []
    square, vstart = binary32('.0035'), binary32('.01694')
    for x, y, z in samples:
        den = x + 15 * y + 3 * z
        u, v = ((4 * x / den, 9 * y / den) if y else
                (D('.210526316'), D('.473684211')))
        if bits == 32:
            le = floor(256 * (y.ln() / D(2).ln() + 64)) if y else 0
            ue, ve = floor(410 * u), floor(410 * v)
            word = le << 16 | ue << 8 | ve
            u, v = (D(ue) + D('.5')) / 410, (D(ve) + D('.5')) / 410
            exponent = (D(le) + D('.5')) / 256 - 64
        else:
            le = floor(64 * (y.ln() / D(2).ln() + 12)) if y else 0
            vi = floor((v - vstart) / square)
            _, start, count, cumulative = next(r for r in UV_ROWS if r[0] == vi)
            start = binary32(start)
            ui = floor((u - start) / square)
            assert 0 <= ui < count
            word = le << 14 | (cumulative + ui)
            u = start + (D(ui) + D('.5')) * square
            v = vstart + (D(vi) + D('.5')) * square
            exponent = (D(le) + D('.5')) / 64 - 12
        luminance = (exponent * D(2).ln()).exp() if le else D(0)
        xyz = [9*u*luminance/(4*v), luminance,
               (12-3*u-20*v)*luminance/(4*v)]
        words.append(word)
        centers.append([binary32(value) for value in xyz])
    return words, centers


def tiff(words, bits, rows=1, orientation=1, tile=False, icc=b'',
         white=False, stonits=False):
    """Serialize prescribed packed words; the only RLE packets are literals."""
    width, height = (16, 16) if tile else (4, 2)
    raster = ([words[0:4] + [words[0]] * 12,
               words[4:8] + [words[0]] * 12] + [[words[0]] * 16] * 14
              if tile else [words[0:4], words[4:8]])
    encoded = []
    for row in raster:
        encoded.append(b''.join(w.to_bytes(3, 'big') for w in row) if bits == 24
                       else b''.join(bytes([width]) + bytes((w >> s) & 255 for w in row)
                                     for s in (24, 16, 8, 0)))
    chunks = ([b''.join(encoded)] if tile else
              [b''.join(encoded[i:i+rows]) for i in range(0, height, rows)])
    offset_tag, count_tag = (324, 325) if tile else (273, 279)
    tags = {256: (4, [4]), 257: (4, [2]), 258: (3, [16]*3),
            259: (3, [34677 if bits == 24 else 34676]), 262: (3, [32845]),
            274: (3, [orientation]), 277: (3, [3]), 284: (3, [1]),
            339: (3, [2]*3), offset_tag: (4, [0]*len(chunks)),
            count_tag: (4, [len(chunk) for chunk in chunks])}
    if tile:
        tags.update({322: (4, [16]), 323: (4, [16])})
    else:
        tags[278] = (4, [rows])
    if icc:
        tags[34675] = (7, icc)
    if white:
        tags[318] = (5, [3127, 10000, 3290, 10000])
    if stonits:
        tags[37439] = (12, [100.0])

    def directory(offsets):
        tags[offset_tag] = (4, offsets)
        start = 8 + 2 + len(tags)*12 + 4
        table, payload = [], bytearray()
        for tag, (kind, values) in sorted(tags.items()):
            data = values if kind == 7 else struct.pack(
                '<' + {3:'H', 4:'I', 5:'I', 12:'d'}[kind]*len(values), *values)
            count = len(values)//2 if kind == 5 else len(values)
            field = data.ljust(4, b'\0') if len(data)<=4 else struct.pack('<I', start+len(payload))
            table.append(struct.pack('<HHI', tag, kind, count)+field)
            if len(data)>4:
                payload.extend(data)
                payload.extend(bytes(len(payload)%2))
        return b'II*\0\x08\0\0\0'+struct.pack('<H',len(tags))+b''.join(table)+bytes(4)+payload
    header = directory([0]*len(chunks))
    offsets, pos = [], len(header)
    for chunk in chunks:
        offsets.append(pos)
        pos += len(chunk)
    return directory(offsets) + b''.join(chunks)


def build(profile, opponents, encode):
    with localcontext() as context:
        context.prec = 90
        return values(profile, opponents, encode)


def values(profile, opponents, encode):
    black_bounds, black_model, black_header = black_lightness_model()
    literal = ['0 0 0', '.2 .2 .2', '1 1 1', '2 2 2',
               '.95047 1 1.08883', '.1 .8 .1', '.45 .2 .02', '.8 4 1']
    samples = [[binary32(v) for v in row.split()] for row in literal]
    primaries = [(D('.64'),D('.33')), (D('.30'),D('.60')), (D('.15'),D('.06'))]
    basis = [[x/y for x,y in primaries], [D(1)]*3,
             [(1-x-y)/y for x,y in primaries]]
    wp = [D('.3127')/D('.3290'), D(1), (1-D('.3127')-D('.3290'))/D('.3290')]
    weights = apply(inverse(basis), wp)
    matrix = inverse([[v*weights[c] for c,v in enumerate(row)] for row in basis])
    all_words, all_xyz, all_limits, all_bytes, all_legacy = [], [], [], [], []
    outputs = {}
    for bits in (32, 24):
        words, xyz = words_and_centers(samples, bits)
        linear = [apply(matrix, row) for row in xyz]
        # A float publication precedes subsequent frame-space conversion.
        gamma = [[encode(binary32(v)) for v in row] for row in linear]
        opponent = [opponents([binary32(v) for v in row]) for row in linear]
        targets = [linear, gamma, [r[1] for r in opponent],
                   [r[0] for r in opponent], [r[2] for r in opponent]]
        limits = []
        for target_index, target in enumerate(targets):
            block = []
            for sample_index, row in enumerate(target):
                row = [v if isinstance(v,D) else D.from_float(v) for v in row]
                budget = max(map(abs,row))*D(64)/2**24
                block.append([[v-budget for v in row], [v+budget for v in row]])
                if sample_index == 0 and target_index in (2, 4):
                    bound = D.from_float(black_bounds[target_index == 4])
                    block[-1][0][0], block[-1][1][0] = -bound, bound
            limits.append(block)
        rgb8 = [[floor(min(D(1), max(D(0), v))*255+D('.5')) for v in row] for row in gamma]
        legacy_matrix = [[D(v) for v in row.split()] for row in
                         ['2.690 -1.276 -.414','-1.022 1.978 .044','.061 -.224 1.163']]
        legacy = [[0 if v<=0 else 255 if v>=1 else floor(256*v.sqrt())
                   for v in apply(legacy_matrix,row)] for row in xyz]
        all_words.append(words); all_xyz.append(xyz); all_limits.append(limits)
        all_bytes.append(rgb8); all_legacy.append(legacy)
        outputs[f'tiff-logluv{bits}.tif'] = tiff(words, bits)
    variants = [('rows2',dict(rows=2)), ('o2',dict(orientation=2)),
                ('o3',dict(orientation=3)), ('o4',dict(orientation=4)),
                ('tile',dict(tile=True)), ('icc',dict(icc=profile)),
                ('white',dict(white=True)), ('stonits',dict(stonits=True))]
    for name, options in variants:
        outputs[f'tiff-logluv32-{name}.tif'] = tiff(all_words[0],32,**options)
    header = ('/* Independent LogLuv code centers and stored-coordinate policy. */\n'
              + array('tlog_xyz', 'float', all_xyz)
              + array('tlog_bounds', 'double', all_limits)
              + array('tlog_bytes', 'unsigned char', all_bytes)
              + array('tlog_legacy', 'unsigned char', all_legacy)
              + array('tlog_icc', 'unsigned char', list(profile)))
    outputs['tiff-logluv.h'] = header.encode()
    outputs['black-lightness.h'] = black_header.encode()
    record = dict(generator='tools/generate_tiff_logluv_tests.py', decimal_digits=90,
        prescribed_xyz=literal, input_float32=samples, words=all_words,
        decoded_xyz=all_xyz, matrix=matrix, bounds=all_limits, bytes=all_bytes,
        legacy_bytes=all_legacy, targets=['linear','gamma','cielab','oklab','din99d'],
        uv_rows=UV_ROWS, codec='NODITHER cell centers; SGILOG24 binary32 codebook constants',
        policy='Stored XYZ and relative exposure; no CAT, ICC apply or StoNits scaling',
        budget='64u32 relative to each target triplet; only black Lab/DIN99d L adds the derived absolute cancellation bound; storage quantization is separate',
        black_lightness=black_model,
        independence='No libtiff/CMM/production output; literal packet TIFF writer and independent XYZ/matrix formulas',
        profile_sha256=hashlib.sha256(profile).hexdigest(),
        fixtures_sha256={name:hashlib.sha256(data).hexdigest() for name,data in outputs.items() if name.endswith('.tif')})
    outputs['tiff-logluv.json'] = (json.dumps(record,indent=2,default=str)+'\n').encode()
    return outputs
