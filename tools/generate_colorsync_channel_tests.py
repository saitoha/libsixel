#!/usr/bin/env python3
"""Independent ColorSync channel-layout controls; never call a CMM."""

from decimal import Decimal as D, localcontext, ROUND_HALF_UP
import hashlib
import itertools
import json
import struct

from generate_colorsync_lab_tests import byte_array
from generate_lcms_input_tests import array as fixture_array
from generate_tiff_icc_tests import read_tiff


def array(name, ctype, values):
    result = fixture_array(name, ctype, values)
    first, newline, rest = result.partition('\n')
    if len(first) > 80:
        first = first.replace(' = ', ' =\n    ', 1)
    return first + newline + rest


def encode(value):
    sign = -1 if value < 0 else 1
    value = abs(value)
    return sign * (D('12.92') * value if value <= D('.0031308') else
        D('1.055') * value ** (D(1) / D('2.4')) - D('.055'))


def transport(bounds):
    lo, hi = [[encode(v) for v in row] for row in bounds]
    margin = max(abs(v) for row in [lo, hi] for v in row) * D(64) / 2**24
    return [[v - margin for v in lo], [v + margin for v in hi]]


def count_profile(count, pack_profile, fixed):
    # Each colorant affects a bounded affine PCS table; there is no native
    # profile builder or measured output in this reproducible fixture.
    lut = b'mft2' + bytes(4) + bytes([count, 3, 2, 0])
    lut += b''.join(fixed(D(i == j)) for i in range(3) for j in range(3))
    lut += struct.pack('>2H', 2, 2) + struct.pack('>2H', 0, 65535) * count
    for values in itertools.product([0, 1], repeat=count):
        xyz = [D('.1') + D('.3') * values[c % count]
               + D('.1') * sum(values) / count for c in range(3)]
        lut += struct.pack('>3H', *[int((v * 32768).to_integral_value())
                                   for v in xyz])
    lut += struct.pack('>2H', 0, 65535) * 3
    return pack_profile([(b'A2B0', lut)], profile_class=b'scnr',
        space=(format(count, 'X') + 'CLR').encode(),
        name='Independent channel-count fixture')


def build(fixtures, pack_profile, fixed):
    with localcontext() as context:
        context.prec = 90
        return build_values(fixtures, pack_profile, fixed)


def build_values(fixtures, pack_profile, fixed):
    existing = {x['format']: x for x in
                json.loads(fixtures['lcms-expanding-alias.json'])['cases']}
    gray_budget = D(str(json.loads(fixtures['gray-samples.json'])[
        'error_contract']['native_relative_budget']))
    lab = json.loads(fixtures['colorsync-lab.json'])
    cases = []
    for fmt in [1, 2, 3, 5, 6, 8]:
        if fmt == 8:
            source = lab['source']
            ideals = [[D(str(v)) for v in x[0]] for x in lab['ideal']]
            intervals = [[[[D(str(v)) for v in row] for row in x[g]]
                          for g in range(2)] for x in lab['bounds']]
            profile = 'lab'
        else:
            entry = existing[fmt]
            source = entry['source']
            ideals = [[D(str(v)) for v in x[0]] for x in entry['expected']]
            intervals = []
            for i, values in enumerate(ideals):
                if fmt == 1:
                    margin = max(map(abs, values)) * gray_budget
                    lo, hi = ([v - margin for v in values],
                              [v + margin for v in values])
                else:
                    lo, hi = [[D(str(v)) for v in row]
                              for row in entry['bounds'][i][0]]
                    extra = D('.0001') - D('.0000005')
                    lo, hi = [v - extra for v in lo], [v + extra for v in hi]
                intervals.append([[lo, hi], transport([lo, hi])])
            profile = 'gray' if fmt == 1 else 'rgb' if fmt in [2, 3] else 'cmyk'
        eligible = [i for i, row in enumerate(ideals)
                    if all(D('.01') < v < D('.99') for v in row)]
        chosen = [eligible[0], eligible[-1]]
        cases.append(dict(format=fmt, profile=profile, sample_indices=chosen,
            source=[source[i] for i in chosen],
            ideal=[[ideals[i], list(map(encode, ideals[i]))] for i in chosen],
            bounds=[intervals[i] for i in chosen]))
    profiles = dict(rgb=fixtures['display-p3.icc'],
        gray=fixtures['gray-gamma2.icc'], cmyk=fixtures['cmyk.icc'],
        lab=read_tiff('lab_high_precision_icc.tiff', True)['profile'],
        xyz=xyz_profile(), clr3=count_profile(3, pack_profile, fixed),
        clr4=count_profile(4, pack_profile, fixed),
        linear=fixtures['colorsync-lab-linear.icc'],
        gamma=fixtures['colorsync-lab-gamma.icc'])
    header = '/* Fixed channel-layout inputs and independent numeric bounds. */\n'
    for name, profile in profiles.items():
        header += byte_array('sc_' + name + '_icc', profile)
    for i, case in enumerate(cases):
        header += array('sc_source_' + str(i), 'double',
            [v + [0] * (4 - len(v)) for v in case['source']])
        header += array('sc_bounds_' + str(i), 'double', case['bounds'])
    header += ('static struct {\n    int format;\n    int channels;\n'
               '    unsigned char const *profile;\n    size_t size;\n'
               '    double const (*source)[4];\n'
               '    double const (*bounds)[2][2][3];\n'
               '} const sc_cases[] = {\n')
    for i, case in enumerate(cases):
        name = 'sc_' + case['profile'] + '_icc'
        header += (f'    {{ {case["format"]}, {len(case["source"][0])}, {name}, '
                   f'sizeof({name}),\n        sc_source_{i}, sc_bounds_{i} }},\n')
    header += '};\n'
    header += ('static struct {\n    unsigned char const *bytes;\n'
               '    size_t size;\n    int colorants;\n'
               '} const sc_profiles[] = {\n')
    for name, count in [('rgb', 3), ('gray', 1), ('cmyk', 4), ('lab', 3),
                        ('xyz', 3), ('clr3', 3), ('clr4', 4)]:
        var = 'sc_' + name + '_icc'
        header += f'    {{ {var}, sizeof({var}), {count} }},\n'
    header += '};\n'
    record = dict(precision_decimal=90, cases=cases,
        profile_sha256={k: hashlib.sha256(v).hexdigest() for k, v in profiles.items()},
        allowance='Existing ColorSync RGB 1e-4, CMYK PCS bins and native Gray relative allowance; monotone gamma transport plus triplet64u32; fixed Lab intervals; no observed-output fitting.')
    scope_header, scope_cases = xyz_controls(profiles)
    outputs = {'colorsync-channels.h': header.encode(),
            'colorsync-channels.json': (json.dumps(record, indent=2,
                                                  default=str) + '\n').encode(),
            'colorsync-channel-xyz.icc': profiles['xyz'],
            'colorsync-channel-3clr.icc': profiles['clr3'],
            'colorsync-channel-4clr.icc': profiles['clr4']}
    outputs['colorsync-channel-scope.h'] = scope_header.encode()
    outputs['colorsync-channel-scope.json'] = (json.dumps(
        dict(cases=scope_cases), indent=2, default=str) + '\n').encode()
    outputs.update(caller_controls(fixtures))
    return outputs


def xyz_profile():
    # Preserve the accepted XYZ-identity envelope from the frozen native
    # reproduction. Its A2B0 has only three identity B curves, independently
    # checked by the numeric generator; this literal needs no CMM to build.
    return bytes.fromhex(
        "000001e46c636d73044000006162737458595a2058595a20"
        "07ea0009000f001700140020616373704150504c00000000"
        "00000000000000000000000000000000000000000000f6d6"
        "000100000000d32d6c636d73000000000000000000000000"
        "000000000000000000000000000000000000000000000000"
        "00000000000000000000000564657363000000c000000046"
        "63707274000001080000004c777470740000015400000014"
        "63686164000001680000002c413242300000019400000050"
        "6d6c756300000000000000010000000c656e55530000002a"
        "0000001c00580059005a0020006900640065006e00740069"
        "007400790020006200750069006c0074002d0069006e0000"
        "6d6c756300000000000000010000000c656e555300000030"
        "0000001c004e006f00200063006f00700079007200690067"
        "00680074002c002000750073006500200066007200650065"
        "006c007958595a20000000000000f6d6000100000000d32d"
        "736633320000000000010000000000000000000000000000"
        "00010000000000000000000000000000000100006d414220"
        "000000000303000000000020000000000000000000000000"
        "000000007061726100000000000000000001000070617261"
        "000000000000000000010000706172610000000000000000"
        "00010000"
    )


def xyz_controls(profiles):
    from generate_tiff_fixtures import apply, inverse, tags
    from generate_colorsync_lab_tests import read_matrix

    matrix = read_matrix(profiles['linear'])
    invert = inverse(matrix)
    identity = tags(profiles['xyz'])[b'A2B0']
    assert identity[:4] == b'mAB ' and identity[8:10] == bytes([3, 3])
    assert struct.unpack_from('>5I', identity, 12) == (32, 0, 0, 0, 0)
    assert identity[32:] == bytes.fromhex(
        '70617261000000000000000000010000') * 3
    intended = [[D('.2'), D('.4'), D('.6')], [D('.7'), D('.3'), D('.1')]]
    xyz = [apply(matrix, v) for v in intended]
    cases = []
    for fmt in [2, 3, 4]:
        source, ideal, bounds = [], [], []
        for i, values in enumerate(xyz):
            if fmt == 4:
                stored = [struct.unpack('=f', struct.pack('=f', float(v)))[0]
                          for v in values]
                actual = list(map(D.from_float, stored))
            else:
                stored = [int((v * 255).to_integral_value(
                    rounding=ROUND_HALF_UP)) for v in values]
                actual = [D(v) / 255 for v in stored]
            source.append(stored + ([11 if i == 0 else 244] if fmt == 3 else []))
            linear = apply(invert, actual)
            assert all(D('.01') < v < D('.99') for v in linear)
            interval = [[v - D('.0001') for v in linear],
                        [v + D('.0001') for v in linear]]
            ideal.append([linear, list(map(encode, linear))])
            bounds.append([interval, transport(interval)])
        cases.append(dict(format=fmt, source=source, ideal=ideal, bounds=bounds))
    header = '/* Independent native XYZ passthrough numeric controls. */\n'
    header += array('sc_xyz_source', 'double',
        [[v + [0] * (4 - len(v)) for v in c['source']] for c in cases])
    header += array('sc_xyz_bounds', 'double', [c['bounds'] for c in cases])
    return header, cases


def psd_integer(profile, samples, depth):
    # One opaque normal layer, no merged image. Store inverted CMYK planes.
    resource = (b'8BIM' + struct.pack('>H', 1039) + bytes(2)
                + struct.pack('>I', len(profile)) + profile)
    resource += bytes(len(resource) % 2)
    header = b'8BPS' + struct.pack('>H', 1) + bytes(6)
    header += struct.pack('>HIIHH', 4, 16, 16, depth, 4)
    header += bytes(4) + struct.pack('>I', len(resource)) + resource
    peak = (1 << depth) - 1
    scalar = 'B' if depth == 8 else 'H'
    planes = [b''.join(struct.pack('>' + scalar, peak - samples[i % 8][c])
                       for i in range(256)) for c in range(4)]
    payload = [bytes(2) + plane for plane in planes]
    record = struct.pack('>iiiiH', 0, 0, 16, 16, 4)
    record += b''.join(struct.pack('>hI', c, len(p))
                       for c, p in enumerate(payload))
    record += b'8BIMnorm' + bytes([255, 0, 0, 0]) + struct.pack('>I', 12)
    record += bytes(12)
    info = struct.pack('>h', 1) + record + b''.join(payload)
    section = struct.pack('>I', len(info)) + info
    return header + struct.pack('>I', len(section)) + section + bytes(2)


def caller_controls(fixtures):
    inks = json.loads(fixtures['cmyk.json'])
    rows = {}
    outputs = {}
    for depth in [8, 16]:
        source = inks['source' if depth == 8 else 'source16']
        peak = (1 << depth) - 1
        values, intervals = [], []
        for row in source:
            unit = [D(x) / peak for x in row]
            gamma_rgb = [(1 - unit[c]) * (1 - unit[3]) for c in range(3)]
            linear = [v / D('12.92') if v <= D('.04045') else
                      ((v + D('.055')) / D('1.055')) ** D('2.4')
                      for v in gamma_rgb]
            margin = max(linear) * D(64) / 2**24
            values.append(linear)
            intervals.append([[v - margin for v in linear],
                              [v + margin for v in linear]])
        rows[str(depth)] = dict(source=source, ideal=values, bounds=intervals)
        if depth == 8:
            rows['8']['expected_rgb8'] = [[
                ((255 - v[c]) * (255 - v[3]) + 127) // 255 for c in range(3)]
                for v in source]
        for kind, profile in [('none', b''), ('valid', fixtures['cmyk.icc']),
                              ('mismatch', fixtures['display-p3.icc'])]:
            outputs[f'colorsync-channel-cmyk{depth}-{kind}.psd'] = \
                psd_integer(profile, source, depth)
    header = '/* Existing typed fallback from the unchanged original inks. */\n'
    header += array('sc_psd8_rgb', 'unsigned char', rows['8']['expected_rgb8'])
    header += array('sc_psd16_bounds', 'double', rows['16']['bounds'])
    outputs['colorsync-channel-caller.h'] = header.encode()
    outputs['colorsync-channel-caller.json'] = (
        json.dumps(dict(cases=rows, model='Depth8 exact nearest-byte gamma RGB; depth16 independent linear sRGB and existing triplet64u32 with exact black; no measured bounds.'),
                   indent=2, default=str) + '\n').encode()
    return outputs
