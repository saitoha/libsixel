#!/usr/bin/env python3
"""Independent value oracles for the original TIFF ICC regression specimens."""

from decimal import Decimal as D, localcontext
from pathlib import Path
import hashlib
import json
import math
import struct
import zlib

from generate_tiff_fixtures import apply, destination_matrix, f32, inverse, tags

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / 'tests/data/colormgmt/input/custom'


def read_tiff(name, floating):
    """Read the original single-strip literals without a TIFF decoder or CMM."""
    data = (INPUT / name).read_bytes()
    assert data[:8] == b'II*\0\x08\0\0\0'
    fields = {}
    for i in range(struct.unpack_from('<H', data, 8)[0]):
        at = 10 + i * 12
        tag, kind, count = struct.unpack_from('<HHI', data, at)
        size = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 7: 1}[kind] * count
        offset = at + 8 if size <= 4 else struct.unpack_from('<I', data, at + 8)[0]
        assert offset + size <= len(data)
        fields[tag] = (kind, count, offset, data[offset:offset + size])

    def numbers(tag):
        kind, count, _, blob = fields[tag]
        return struct.unpack('<' + {3: 'H', 4: 'I'}[kind] * count, blob)

    width, height = numbers(256)[0], numbers(257)[0]
    assert numbers(277) == (3,) and numbers(284) == (1,)
    assert 274 not in fields and 317 not in fields
    assert len(numbers(273)) == len(numbers(279)) == 1
    offset, size = numbers(273)[0], numbers(279)[0]
    raster = data[offset:offset + size]
    if floating:
        assert numbers(258) == (32, 32, 32) and numbers(262) == (8,)
        assert numbers(259) == (1,) and numbers(339) == (3, 3, 3)
        samples = list(struct.iter_unpack('<fff', raster))
    else:
        assert numbers(258) == (8, 8, 8) and numbers(262) == (2,)
        assert numbers(259) == (32946,)
        samples = list(struct.iter_unpack('BBB', zlib.decompress(raster)))
    assert len(samples) == width * height
    _, _, offset, profile = fields[34675]
    return dict(file=name, width=width, height=height, samples=samples,
                profile=profile, profile_offset=offset,
                file_sha256=hashlib.sha256(data).hexdigest())


def clip(value):
    return max(D(0), min(D(1), value))


def parameters(curve):
    assert curve[:4] == b'para'
    return [D(x) / 65536 for x in struct.unpack(
        '>' + 'i' * ((len(curve) - 12) // 4), curve[12:])]


def evaluate(curve, value):
    """ICC.1:2022 Table 68, including the mandated function-range clipping."""
    kind = struct.unpack_from('>H', curve, 8)[0]
    p = parameters(curve)
    assert kind in (3, 4) and D(0) <= value <= D(1)
    g, a, b, c, d = p[:5]
    e, f = p[5:] if kind == 4 else (D(0), D(0))
    result = (a * value + b) ** g + e if value >= d else c * value + f
    return clip(result)


def lab_xyz(sample):
    """Physical Lab under PCS D50, independently of the facade/LUT adapter."""
    lightness, a, b = sample
    fy = (lightness + 16) / 116
    fx, fz = fy + a / 500, fy - b / 200
    delta = D(6) / 29

    def inv(value):
        return value ** 3 if value > delta else 3 * delta ** 2 * (value - D(4) / 29)

    return [D('.9642') * inv(fx), inv(fy), D('.8249') * inv(fz)]


def gamma(value):
    sign = -1 if value < 0 else 1
    value = abs(value)
    return sign * (D('12.92') * value if value <= D('.0031308') else
                   D('1.055') * value ** (D(1) / D('2.4')) - D('.055'))


def c_array(name, ctype, rows, formatter):
    result = f'static {ctype} const {name}[{len(rows)}][3] = {{\n'
    for row in rows:
        values = [formatter(v) for v in row]
        line = '    { ' + ', '.join(values) + ' },'
        result += (line + '\n' if len(line) <= 80 else
                   '    {\n' + ''.join(f'        {v},\n' for v in values) + '    },\n')
    return result + '};\n'


def png_rgb8(width, height, pixels):
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload)))

    stride = width * 3
    raster = b''.join(b'\0' + pixels[i:i + stride] for i in range(0, len(pixels), stride))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(
        '>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + chunk(b'sRGB', b'\0')
        + chunk(b'IDAT', zlib.compress(raster)) + chunk(b'IEND', b''))


def build():
    with localcontext() as context:
        context.prec = 80
        return build_values()


def build_values():
    rgb = read_tiff('rgb_parametric_34.tiff', False)
    lab = read_tiff('lab_high_precision_icc.tiff', True)
    rgb_tags, lab_tags = tags(rgb['profile']), tags(lab['profile'])
    # These original RGB headers are deliberately permissive parser specimens;
    # neither the oracle nor its quality reference claims ICC conformance.
    assert rgb['profile'][8:16] == bytes(8)
    assert lab['profile'][8:24] == bytes.fromhex('02200000') + b'spacLab Lab '
    for tag in (b'A2B0', b'B2A0'):
        lut = lab_tags[tag]
        assert lut[:4] == b'mft1' and lut[8:11] == bytes([3, 3, 2])
        assert struct.unpack_from('>9i', lut, 12) == (65536, 0, 0, 0, 65536, 0, 0, 0, 65536)
        assert lut[48:816] == lut[840:] == bytes(range(256)) * 3
        assert lut[816:840] == bytes(v for a in (0, 255) for b in (0, 255)
                                    for c in (0, 255) for v in (a, b, c))
    destination_inverse = inverse(destination_matrix())
    source_matrix = list(map(list, zip(*[[D(v) / 65536 for v in struct.unpack_from(
        '>iii', rgb_tags[c + b'XYZ'], 8)] for c in (b'r', b'g', b'b')])))
    curves = [rgb_tags[c + b'TRC'] for c in (b'r', b'g', b'b')]
    rgb_linear = [apply(destination_inverse, apply(source_matrix, [
        evaluate(curve, D(v) / 255) for curve, v in zip(curves, pixel)]))
        for pixel in rgb['samples']]
    curve_cases = []
    for channel, curve in enumerate(curves):
        threshold = float(parameters(curve)[4])
        inputs = {v[channel] / 255.0 for v in rgb['samples']}
        inputs.update(x for x in (threshold, math.nextafter(threshold, -math.inf),
                                 math.nextafter(threshold, math.inf), 0.0, 1.0)
                      if 0.0 <= x <= 1.0)
        for x in sorted(inputs):
            curve_cases.append((channel, x, float(evaluate(curve, D.from_float(x)))))
    normalized_lab = [[D.from_float(x) for x in p] for p in lab['samples']]
    lab_device = [[p[0], (p[1] * 128 + 128) / 255, (p[2] * 128 + 128) / 255]
                  for p in normalized_lab]
    lab_pcs = [lab_xyz([p[0] * 100, p[1] * 128, p[2] * 128]) for p in normalized_lab]
    lab_linear = [apply(destination_inverse, p) for p in lab_pcs]
    lab_gamma = [[gamma(x) for x in p] for p in lab_linear]
    header = ('/* Original TIFF/ICC values; independent formulas, no CMM oracle. */\n'
              '#define TIFF_ICC_RGB_COUNT 1024\n#define TIFF_ICC_LAB_COUNT 256\n'
              f'#define TIFF_ICC_CURVE_COUNT {len(curve_cases)}\n'
              'static double const tiff_icc_double_relative = 64.0 / 9007199254740992.0;\n'
              'static double const tiff_icc_float_relative = 64.0 / 16777216.0;\n'
              f'static long const tiff_icc_offsets[2] = {{ {rgb["profile_offset"]}, {lab["profile_offset"]} }};\n'
              f'static size_t const tiff_icc_sizes[2] = {{ {len(rgb["profile"])}, {len(lab["profile"])} }};\n'
              'static struct {\n    int channel;\n    double input;\n'
              '    double expected;\n} const tiff_icc_curves[] = {\n')
    header += ''.join(f'    {{ {c}, {x:.17g}, {y:.17g} }},\n' for c, x, y in curve_cases) + '};\n'
    header += c_array('tiff_icc_rgb_source', 'unsigned char', rgb['samples'], str)
    header += c_array('tiff_icc_lab_source', 'float', lab['samples'], lambda x: f'{x:.17g}')
    for name, values, round_f32 in [('rgb_linear', rgb_linear, True), ('lab_device', lab_device, False),
                                   ('lab_pcs', lab_pcs, False), ('lab_linear', lab_linear, True),
                                   ('lab_gamma', lab_gamma, True)]:
        header += c_array('tiff_icc_' + name, 'double', values,
                          lambda x: f'{f32(x) if round_f32 else float(x):.17g}')
    pixels = bytes(int((gamma(clip(v)) * 255 + D('.5')).to_integral_value(rounding='ROUND_FLOOR'))
                   for pixel in rgb_linear for v in pixel)
    manifest = dict(precision_decimal=80, rgb_source='rgb_parametric_34.tiff',
        lab_source='lab_high_precision_icc.tiff', rgb_sha256=rgb['file_sha256'],
        lab_sha256=lab['file_sha256'], curve_cases=curve_cases,
        rgb_linear_f32=[[f32(v) for v in p] for p in rgb_linear],
        lab_pcs=[[str(v) for v in p] for p in lab_pcs],
        lab_linear_f32=[[f32(v) for v in p] for p in lab_linear],
        lab_gamma_f32=[[f32(v) for v in p] for p in lab_gamma],
        rgb_reference_pixels_sha256=hashlib.sha256(pixels).hexdigest(),
        float_relative=64 / 2**24, double_relative=64 / 2**53,
        sources=['https://www.color.org/specifications/ICC.1-2022-05.pdf#page=85',
                 'https://www.color.org/specification/ICC.1-2001-04.pdf#page=58',
                 'https://www.color.org/specification/ICC.1-2001-04.pdf#page=76'],
        note='Zero-version RGB parser specimen; identity Lab v2 LUT8; original generators unknown')
    return {'tiff-icc.h': header.encode(),
            'tiff-icc.json': (json.dumps(manifest, indent=2) + '\n').encode(),
            'tiff-para34-expected.png': png_rgb8(rgb['width'], rgb['height'], pixels)}
