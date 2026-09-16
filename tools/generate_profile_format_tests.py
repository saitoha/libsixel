#!/usr/bin/env python3
"""Independent matched-profile controls and original-input PSD fallback."""
from decimal import Decimal as D, localcontext
import hashlib
import json
import struct
from generate_colorsync_channel_tests import array
from generate_colorsync_lab_tests import byte_array
from generate_lcms_input_tests import psd32
from generate_tiff_icc_tests import read_tiff
u = D(64) / D(2) ** 24

def dec(x):
    return D(str(x))

def gamma(x):
    """Signed analytic sRGB inverse for the already bounded reference values."""
    s = -1 if x < 0 else 1
    x = abs(x)
    return s * (D('12.92') * x if x <= D('.0031308') else D('1.055') * x ** (D(1) / D('2.4')) - D('.055'))

def transport(bounds):
    """Transport the existing interval monotonically and reserve the existing 64u32 gamma/storage margin."""
    lo, hi = [[gamma(v) for v in row] for row in bounds]
    margin = max((abs(v) for row in [lo, hi] for v in row)) * u
    return [[x - margin for x in lo], [x + margin for x in hi]]

def build(fixtures):
    """Regenerate the original 54 matched controls from independent fixture records; selection and existing allowances are unchanged."""
    load = lambda name: json.loads(fixtures[name])
    ea = {x['format']: x for x in load('lcms-expanding-alias.json')['cases']}
    units = load('lcms-input-units.json')
    cs = load('colorsync-lab.json')
    tiff = load('tiff-icc.json')
    gray = load('gray-samples.json')['error_contract']
    rows = []
    with localcontext() as ctx:
        ctx.prec = 90
        for engine in [2, 3, 4]:
            for fmt in range(1, 10):
                if fmt in ea or fmt == 4:
                    c = ea[2 if fmt == 4 else fmt]
                    source = c['source']
                    ideals = [[dec(v) for v in row[0]] for row in c['expected']]
                    profile = c['profile']
                    bounds = []
                    for i, v in enumerate(ideals):
                        if fmt in [1, 9]:
                            budget = dec(gray['native_relative_budget']) if engine == 4 else u
                            margin = max((abs(x) for x in v)) * budget
                            lo = [x - margin for x in v]
                            hi = [x + margin for x in v]
                            origin = 'CMS0045 relative Gray allowance; ' + ('native matrix plus float' if engine == 4 else '64u32')
                        else:
                            lo, hi = [[dec(x) for x in row] for row in c['bounds'][i][0]]
                            if engine == 4:
                                extra = D('.0001') - D('.0000005')
                                lo = [x - extra for x in lo]
                                hi = [x + extra for x in hi]
                            origin = 'CMS0025/0015 RGB or CMS0033 CMYK allowance; native ColorSync 1e-4 or float 5e-7, plus existing CMYK PCS bins'
                            if engine == 2 and fmt in [5, 6]:
                                lo = [x - D('.0000005') for x in v]
                                hi = [x + D('.0000005') for x in v]
                        bounds.append([lo, hi])
                    if fmt == 4:
                        source = [[struct.unpack('=f', struct.pack('=f', float(x) / 255))[0] for x in row[:3]] for row in source]
                elif fmt == 7:
                    profile = 'cmyk.icc'
                    source = units['cmyk']['source']
                    ideals = [[dec(v) for v in x[0]] for x in units['cmyk']['ideal']]
                    bounds = []
                    for i, v in enumerate(ideals):
                        if engine == 2:
                            margin = max((abs(x) for x in v)) * u
                            lo = [x - margin for x in v]
                            hi = [x + margin for x in v]
                            origin = 'CA01 builtin affine CMYK float triplet 64u32'
                        else:
                            lo, hi = [[dec(x) for x in row] for row in units['cmyk']['bounds'][i][0]]
                            if engine == 4:
                                lo = [x - D('.0001') for x in lo]
                                hi = [x + D('.0001') for x in hi]
                            origin = 'CMS0033 affine u1.15 PCS/input propagation and float allowance; ColorSync adds existing 1e-4 profile budget'
                        bounds.append([lo, hi])
                else:
                    assert fmt == 8
                    profile = 'lab-original.icc'
                    source = cs['source'] if engine == 4 else units['lab']['source']
                    ideals = [[dec(v) for v in row] for row in tiff['lab_linear_f32']] if engine == 2 else [[dec(v) for v in row[0]] for row in (cs['ideal'] if engine == 4 else units['lab']['ideal'])]
                    bounds = []
                    for i, v in enumerate(ideals):
                        if engine == 2:
                            margin = max((abs(x) for x in v)) * u
                            lo = [x - margin for x in v]
                            hi = [x + margin for x in v]
                            origin = 'CMS0231 original Lab-to-XYZ/sRGB and triplet 64u32'
                        else:
                            lo, hi = [[dec(x) for x in row] for row in (cs['bounds'] if engine == 4 else units['lab']['bounds'])[i][0]]
                            origin = 'Existing CA04 fixed-target Lab intervals' if engine == 4 else 'Existing LCMS Lab encoded-input interval'
                        bounds.append([lo, hi])
                eligible = [i for i, v in enumerate(ideals) if all((D('.01') < x < D('.99') for x in v))]
                assert len(eligible) >= 2, (engine, fmt, eligible)
                selected = [eligible[0], eligible[-1]]
                for g in [0, 1]:
                    target = 'factory-srgb' if g else 'factory-linear'
                    chosen = []
                    ex = []
                    for i in selected:
                        ex.append([gamma(x) for x in ideals[i]] if g else ideals[i])
                        if fmt == 8 and engine in [3, 4]:
                            bb = (cs['bounds'] if engine == 4 else units['lab']['bounds'])[i][g]
                            chosen.append([[dec(x) for x in row] for row in bb])
                        else:
                            chosen.append(transport(bounds[i]) if g else bounds[i])
                    if fmt == 8 and engine == 4:
                        target = 'colorsync-lab-gamma.icc' if g else 'colorsync-lab-linear.icc'
                    rows.append(dict(engine=engine, format=fmt, gamma=g, source_profile=profile, target=target, sample_indices=selected, source=[source[i] for i in selected], ideal=ex, bounds=chosen, allowance_origin=origin))
    record = dict(selection='First and last existing independent specimen whose linear ideal has every component strictly between .01 and .99. Chosen before positive native observations; range/gamut behavior is outside these rejection controls and retained by existing tests.', arithmetic='90-digit Decimal propagation from previously independently verified ICC/sample records; no CMM calls', cases=rows, source_records={n: hashlib.sha256(fixtures[n]).hexdigest() for n in ['lcms-expanding-alias.json', 'lcms-input-units.json', 'builtin-device.json', 'colorsync-lab.json', 'tiff-icc.json', 'gray-samples.json']})
    profiles = {name: fixtures[name] for name in ['display-p3.icc', 'gray-gamma2.icc', 'cmyk.icc', 'cli-rgb.icc', 'colorsync-lab-linear.icc', 'colorsync-lab-gamma.icc']}
    profiles['lab-original.icc'] = read_tiff('lab_high_precision_icc.tiff', True)['profile']
    profiles['xyz.icc'] = fixtures['colorsync-channel-xyz.icc']
    profile_names = list(profiles)
    header = '/* Fixed source samples and independently derived intervals. */\n'
    for i, profile in enumerate(profiles.values()):
        header += byte_array('pf_icc_' + str(i), profile)
    header += 'static struct {\n    unsigned char const *bytes;\n    size_t size;\n} const pf_profiles[] = {\n'
    for i in range(len(profiles)):
        header += f'    {{ pf_icc_{i}, sizeof(pf_icc_{i}) }},\n'
    header += '};\n'
    xyz = next((x for x in json.loads(fixtures['colorsync-channel-scope.json'])['cases'] if x['format'] == 4))
    header += array('pf_xyz_source', 'double', xyz['source'])
    header += array('pf_xyz_bounds', 'double', xyz['bounds'])
    header += array('pf_source', 'double', [[v + [0] * (4 - len(v)) for v in x['source']] for x in rows])
    header += array('pf_bounds', 'double', [x['bounds'] for x in rows])
    header += 'static struct {\n    int engine;\n    int format;\n    int gamma;\n    int channels;\n    int profile;\n    int target;\n    int expected_domain;\n} const pf_cases[] = {\n'
    domains = {1: 2, 2: 1, 3: 1, 4: 1, 5: 3, 6: 3, 7: 3, 8: 4, 9: 2}
    for x in rows:
        target = -1 if x['target'] == 'factory-linear' else -2 if x['target'] == 'factory-srgb' else profile_names.index(x['target'])
        header += '    { %d, %d, %d, %d, %d, %d, %d },\n' % (x['engine'], x['format'], x['gamma'], len(x['source'][0]), profile_names.index(x['source_profile']), target, domains[x['format']])
    header += '};\n'
    outputs = {'profile-format.h': header.encode(), 'profile-format.json': (json.dumps(record, indent=2, default=str) + '\n').encode()}
    outputs.update(caller_controls(fixtures))
    return outputs

def caller_controls(fixtures):
    """Derive the existing fallback from original dyadic inks; transport established ColorSync ICC intervals for the correct-profile controls."""
    with localcontext() as context:
        context.prec = 90
        units = json.loads(fixtures['lcms-input-units.json'])['cmyk']
        source = [[D.from_float(float(v)) for v in row] for row in units['source'][8:16]]
        linear = []
        for row in source:
            rgb = [(1 - row[c]) * (1 - row[3]) for c in range(3)]
            linear.append([v / D('12.92') if v <= D('.04045') else ((v + D('.055')) / D('1.055')) ** D('2.4') for v in rgb])
        cases, correct, outputs, files = ({}, [], {}, {})
        a = D(128) / 255
        valid = [[[D(str(v)) + (-D('.0001') if side == 0 else D('.0001')) for v in bounds[side]] for side in range(2)] for bounds in [row[0] for row in units['bounds'][8:16]]]
        for layers, label in enumerate(['main', 'missing', 'layers']):
            values = [[a * linear[(i + 3) % 8][c] + (1 - a) * linear[i % 8][c] for c in range(3)] if layers == 2 else linear[i % 8] for i in range(256)]
            bounds = []
            valid_bounds = []
            for i, row in enumerate(values):
                margin = max(map(abs, row)) * D(64) / 2 ** 24
                bounds.append([[v - margin for v in row], [v + margin for v in row]])
                valid_bounds.append([[a * valid[(i + 3) % 8][s][c] + (1 - a) * valid[i % 8][s][c] for c in range(3)] for s in range(2)] if layers == 2 else valid[i % 8])
            cases[label] = dict(ideal=values, bounds=bounds)
            correct.append(valid_bounds)
            for kind, profile in [('valid', fixtures['cmyk.icc']), ('mismatch', fixtures['cli-rgb.icc']), ('none', b'')]:
                data = psd32(profile, [[float(v) for v in row] for row in source], layers)
                name = 'profile-format-' + label + '-' + kind + '.psd'
                outputs[name] = data
                files[label + '-' + kind] = hashlib.sha256(data).hexdigest()
        record = dict(source=source, cases=cases, fixture_sha256=files, model='Original inverted CMYK32 dyadic samples; device fallback gamma RGB=(1-CMY)*(1-K); analytic bounded sRGB decode; two layers use top opacity128/255, sample shift3, linear source-over.', margin='Existing float triplet 64/2^24, black exact, no new fitted absolute floor', interpretation='Independent expected existing fallback behavior; not a colorimetric reference for the mismatched ICC. Original fallback outputs were already inspected; this bound comes from the existing float arithmetic policy, not a measured maximum.')
        outputs['profile-format-caller.json'] = (json.dumps(record, indent=2, default=str) + '\n').encode()
        header = '/* Original-input fallback and existing ICC interval controls. */\n'
        header += array('pf_psd_fallback', 'double', [x['bounds'] for x in cases.values()])
        header += array('pf_psd_valid', 'double', correct)
        outputs['profile-format-caller.h'] = header.encode()
        outputs['profile-format-valid-caller.json'] = (json.dumps(dict(bounds=correct, source='Existing lcms-input-units CMYK linear intervals plus existing ColorSync 1e-4 budget, transported through linear source-over with original opacity128/255.'), indent=2, default=str) + '\n').encode()
        return outputs
