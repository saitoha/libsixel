#!/usr/bin/env python3
"""Independent device-unit inputs and ICC precision intervals; no CMM oracle."""

from decimal import Decimal as D, localcontext
import hashlib
import itertools
import json
import struct

from generate_tiff_fixtures import apply, destination_matrix, f32, inverse, tags
from generate_tiff_icc_tests import lab_xyz, png_rgb8, read_tiff


def srgb(value):
    # The native sRGB parametric inverse has a linear negative extension.
    return (D('12.92') * value if value <= D('.0031308') else
            D('1.055') * value ** (D(1) / D('2.4')) - D('.055'))


def array(name, ctype, rows):
    shape = []
    item = rows
    while isinstance(item, list):
        shape.append(len(item))
        item = item[0]

    def render(value, level):
        if not isinstance(value, list):
            return format(float(value), '.17g')
        parts = [render(v, level + 1) for v in value]
        line = '{ ' + ', '.join(parts) + ' }'
        if '\n' not in line and len(line) + level * 4 < 78:
            return line
        pad = '    ' * level
        return '{\n' + ''.join(pad + '    ' + p + ',\n' for p in parts) + pad + '}'

    return ('static ' + ctype + ' const ' + name + ''.join(f'[{n}]' for n in shape)
            + ' = ' + render(rows, 0) + ';\n')


def build(profile, record_bytes):
    with localcontext() as context:
        context.prec = 80
        return build_values(profile, json.loads(record_bytes))


def psd32(profile, samples, layers):
    """Small raw CMYK32 PSD: a composite, one layer, or two normal layers."""
    resource = b'8BIM' + struct.pack('>H', 1039) + bytes(2)
    resource += struct.pack('>I', len(profile)) + profile
    resource += bytes(len(resource) % 2)
    header = b'8BPS' + struct.pack('>H', 1) + bytes(6)
    header += struct.pack('>HIIHH', 4, 16, 16, 32, 4)
    header += bytes(4) + struct.pack('>I', len(resource)) + resource

    def planes(shift):
        return [b''.join(struct.pack('>f', 1 - samples[(i + shift) % 8][c])
                         for i in range(256)) for c in range(4)]

    if layers == 0:
        return header + bytes(6) + b''.join(planes(0))
    records, payloads = b'', b''
    # PSD records are top-to-bottom. The two-layer case blends the shifted
    # top at opacity 128 over an opaque base, in linear light.
    for shift, opacity in ([(0, 255)] if layers == 1 else [(3, 128), (0, 255)]):
        payload = [bytes(2) + p for p in planes(shift)]
        record = struct.pack('>iiiiH', 0, 0, 16, 16, 4)
        record += b''.join(struct.pack('>hI', c, len(p)) for c, p in enumerate(payload))
        extra = bytes(12)  # Empty mask, blending ranges and Pascal name.
        record += b'8BIMnorm' + bytes([opacity, 0, 0, 0])
        records += record + struct.pack('>I', len(extra)) + extra
        payloads += b''.join(payload)
    info = struct.pack('>h', layers) + records + payloads
    section = struct.pack('>I', len(info)) + info
    return header + struct.pack('>I', len(section)) + section + bytes(2)


def build_values(profile, record):
    matrix = inverse(destination_matrix())
    base = record['base_u1_15']
    weights = record['weights_u1_15']
    lut = tags(profile)[b'A2B1']
    assert lut[:4] == b'mft2' and lut[8:11] == bytes([4, 3, 2])
    assert struct.unpack_from('>HH', lut, 48) == (2, 2)
    assert lut[52:68] == struct.pack('>HH', 0, 65535) * 4
    vertices = [[base[j] - sum(b * weights[k][j] for k, b in enumerate(bits))
                 for j in range(3)] for bits in itertools.product((0, 1), repeat=4)]
    assert lut[68:164] == b''.join(struct.pack('>HHH', *v) for v in vertices)
    assert lut[164:] == struct.pack('>HH', 0, 65535) * 3
    cmyk = [[f32(D(v) / 65535) for v in row] for row in record['source16']]
    # Add exact dyadic values for PSD32's inverted storage, dark samples and
    # unequal inks. The public caller fixture uses these unchanged values.
    cmyk += [[0, 1, 1, 0], [1, 0, 1, 0], [1, 1, 0, 0], [0, 0, 0, 0],
             [1, 1, 1, 1], [.25, .5, .75, .125], [.5, .5, .5, 0],
             [1, 0, 0, 1], [1 - 2**-16, 1 - 2**-16, 1 - 2**-16, 0]]
    lab_file = read_tiff('lab_high_precision_icc.tiff', True)
    lab = [list(row) for row in lab_file['samples']]
    lab_lut = tags(lab_file['profile'])[b'A2B0']
    assert lab_lut[:4] == b'mft1'
    assert lab_lut[48:816] == lab_lut[840:] == bytes(range(256)) * 3
    assert lab_lut[816:840] == bytes(v for p in itertools.product((0, 255), repeat=3)
                                     for v in p)
    header = ('/* Generated device units and independent ICC precision intervals. */\n'
              '/* Both ideal and more accurate native evaluations are accepted. */\n')
    results = {}
    for name, inputs in [('cmyk', cmyk), ('lab', lab)]:
        ideals, bounds = [], []
        for sample in inputs:
            device = list(map(D.from_float, map(float, sample)))
            if name == 'cmyk':
                xyz = [(D(base[j]) - sum(device[k] * weights[k][j]
                                        for k in range(4))) / 32768 for j in range(3)]
                # Two u1.15 PCS bins match the existing affine LUT tests.
                # Also propagate one input grid bin through the actual affine
                # weights, including float percent packing/unpacking.
                delta = [D(2) / 32768 + sum(abs(w[j]) for w in weights)
                         / D(32768 * 65535) for j in range(3)]
                xyz_lo = [v - e for v, e in zip(xyz, delta)]
                xyz_hi = [v + e for v, e in zip(xyz, delta)]
            else:
                physical = [device[0] * 100, device[1] * 128, device[2] * 128]
                xyz = lab_xyz(physical)
                # One encoded Lab grid bin covers the two half-step table
                # rounding boundaries. Propagate component intervals through
                # monotone Lab-to-XYZ, then through the signed matrix rows.
                dl, dab = D(100) / 65535, D(255) / 65535
                lightness, a, b = physical
                xyz_lo = lab_xyz([lightness - dl, a - dab, b + dab])
                xyz_hi = lab_xyz([lightness + dl, a + dab, b - dab])
            ideal = apply(matrix, xyz)
            lo = [sum(v * (xyz_lo[j] if v >= 0 else xyz_hi[j])
                      for j, v in enumerate(row)) for row in matrix]
            hi = [sum(v * (xyz_hi[j] if v >= 0 else xyz_lo[j])
                      for j, v in enumerate(row)) for row in matrix]
            # Binary32 arithmetic allowance is scaled per triplet, not fitted
            # to a measured native maximum. No new absolute dark floor.
            margin = max(map(abs, ideal)) * D(64) / 2**24
            lo, hi = [v - margin for v in lo], [v + margin for v in hi]
            ideals.append([list(map(float, ideal)), list(map(float, map(srgb, ideal)))])
            bounds.append([[list(map(float, lo)), list(map(float, hi))],
                           [list(map(float, map(srgb, lo))),
                            list(map(float, map(srgb, hi)))]])
        header += array('cms_units_' + name + '_source', 'float', inputs)
        header += array('cms_units_' + name + '_bounds', 'double', bounds)
        results[name] = {'source': inputs, 'ideal': ideals, 'bounds': bounds}
    for name, data in [('cmyk', profile), ('lab', lab_file['profile'])]:
        header += f'static unsigned char const cms_units_{name}_icc[] = {{\n'
        header += ''.join('    ' + ', '.join(f'0x{v:02x}' for v in data[i:i+12])
                          + ',\n' for i in range(0, len(data), 12)) + '};\n'
    results['precision'] = {'decimal_digits': 80, 'lab_encoded_bins': 1,
                            'cmyk_input_bins': 1, 'cmyk_xyz_bins': 2,
                            'float_relative': 64 / 2**24,
                            'gamma': 'Native sRGB inverse, linear negative extension',
                            'acceptance': 'Independent interval includes unquantized ideal'}
    results['profiles'] = {'cmyk': hashlib.sha256(profile).hexdigest(),
                           'lab': hashlib.sha256(lab_file['profile']).hexdigest()}
    outputs = {'lcms-input-units.h': header.encode()}
    caller_ideals = {}
    for layers, name in enumerate(['main', 'missing', 'layers']):
        outputs[f'lcms-cmyk32-{name}.psd'] = psd32(profile, cmyk[8:16], layers)
        ideal = [results['cmyk']['ideal'][8 + i % 8][0] for i in range(256)]
        if layers == 2:
            alpha = D.from_float(f32(128 / 255))
            ideal = [[float(D.from_float(results['cmyk']['ideal'][8 + (i + 3) % 8][0][c])
                            * alpha + D.from_float(ideal[i][c]) * (1 - alpha))
                      for c in range(3)] for i in range(256)]
        caller_ideals[name] = ideal
        pixels = bytes(int(srgb(max(D(0), min(D(1), D.from_float(v)))) * 255 + D('.5'))
                       for row in ideal for v in row)
        outputs[f'lcms-cmyk32-{name}-reference.png'] = png_rgb8(16, 16, pixels)
    results['psd_callers'] = {'source_indices': list(range(8, 16)),
                              'top_layer_shift': 3, 'top_opacity': 128,
                              'linear_ideal': caller_ideals,
                              'fixtures_sha256': {name: hashlib.sha256(data).hexdigest()
                                                 for name, data in outputs.items()
                                                 if name.endswith('.psd')}}
    outputs['lcms-input-units.json'] = (json.dumps(results, indent=2) + '\n').encode()
    return outputs
