#!/usr/bin/env python3
"""Tiny managed TIFF storage; expected values use a separate byte reader."""
import hashlib
import json
import struct
from fractions import Fraction as F
from generate_tiff_float_tests import write_tiff
import tiff_managed_reference as ref

def build(p3, gray):
    outputs, records = ({}, [])
    rgb = [0.2, 0.4, 0.6, 0.7, 0.2, 0.3, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0.001, 0.01, 0.04, 0.04045, 0.04046, 0.3, 0, 0, 0]
    black = [0, 0.001, 0.01, 0.03, 0.04, 0.125, 0.25, 0.5, 0.75, 1, 0.0001, 0.9] * 2
    extended = [-0.25, 0.5, 1.25, 1.25, 0.5, -0.25] + rgb[6:]
    mixed = [0.001, 0.01, -0.25, 1.25] + black[4:]
    # Replace serialized TRCs without using any CMM.
    entries = {}
    count = struct.unpack_from('>I', p3, 128)[0]
    for index in range(count):
        name, offset, size = struct.unpack_from('>4sII', p3, 132+12*index)
        entries[name] = p3[offset:offset+size]
    curve = b'curv' + bytes(4) + struct.pack('>IHH', 2, 0, 65535)
    entries = {k: curve if k in (b'rTRC', b'gTRC', b'bTRC') else v for k, v in entries.items()}
    start = 132 + 12 * len(entries)
    directory, payload = (bytearray(), bytearray())
    for k, v in entries.items():
        directory += struct.pack('>4sII', k, start + len(payload), len(v))
        payload += v
        payload += bytes(-len(payload) % 4)
    table = bytearray(p3[:128]) + struct.pack('>I', len(entries)) + directory + payload
    struct.pack_into('>I', table, 0, len(table))
    table[84:100] = bytes(16)
    specs = [('rgb', rgb, 2, p3, '<', 1, None), ('black', black, 1, gray, '<', 1, None), ('white', black, 0, gray, '<', 1, None), ('rgb-be-o3', rgb, 2, p3, '>', 3, None), ('white-be-o4', black, 0, gray, '>', 4, None), ('table', rgb, 2, bytes(table), '<', 1, None), ('mixed', mixed, 1, gray, '<', 1, None), ('extended', extended, 2, p3, '<', 1, None), ('rgb-nan', rgb, 2, p3, '<', 1, 2143363909), ('black-pinf', black, 1, gray, '<', 1, 2139095040), ('white-ninf', black, 0, gray, '>', 1, 4286578688)]
    # Reuse the reviewed six-word Gray16 fixtures, with both polarities.
    words16 = [0, 1, 128, 4096, 32768, 65535]
    specs = [(*spec, 32) for spec in specs]
    specs += [('gray16-black', words16, 1, gray, '<', 1, None, 16),
              ('gray16-white', words16, 0, gray, '<', 1, None, 16)]
    eps = F(64, 2 ** 24)
    norm = max((sum(map(abs, row)) for row in ref.D))
    # A half-LSB table input error is amplified by the source matrix first.
    # P3's maximum absolute row sum is below two, so one full LSB before
    # the destination inverse bounds that input quantization separately.
    source_rows = [[F(v, 65536) for v in struct.unpack('>iii',
                    entries[key][8:20])]
                   for key in (b'rXYZ', b'gXYZ', b'bXYZ')]
    source_norm = max(sum(abs(column[row]) for column in source_rows)
                      for row in range(3))
    assert source_norm < 2

    q = F(1, 65536)
    matrix = norm * 4 * q / (1 - norm * 3 * q)
    native = (1 + matrix) * (1 + eps) - 1
    header = '/* Offline independent ICC/TIFF-byte oracle; no CMM values. */\n#ifndef SIXEL_TIFF_MANAGED_VALUES_H\n#define SIXEL_TIFF_MANAGED_VALUES_H\n#include <stdint.h>\nstatic double const tm_float_relative = %.17g;\nstatic double const tm_native_relative = %.17g;\nstatic double const tm_table_absolute = %.17g;\n' % (float(eps), float(native), float(norm / F(65535)))
    header += 'static struct {\n    char const *file;\n    int width;\n    int height;\n    int photo;\n    int bits;\n    uint32_t stored[24];\n    uint32_t source[72];\n    double expected[2][72];\n} const tm_cases[] = {\n'
    for name, values, photo, profile, endian, orientation, poison, bits in specs:
        words = values if bits == 16 else [struct.unpack('>I', struct.pack('>f', x))[0] for x in values]
        if poison is not None:
            words[-1] = poison
        blob = write_tiff(words, photo, endian, orientation, icc=profile,
                          bits=bits, height=1 if bits == 16 else None)
        filename = 'tiff-managed-' + name + '.tif'
        outputs[filename] = blob
        if poison is not None:
            continue
        parsed = ref.tiff(blob)
        source = [struct.pack('>f', v).hex() for rgb_value in parsed['triples'] for v in rgb_value]
        expected = [ref.expected(parsed, clamp) for clamp in [True, False]]
        header += '    { "%s", %d, %d, %d, %d, {\n' % (filename, parsed['width'], parsed['height'], photo, bits)
        header += ',\n'.join(('        0x%08xu' % v for v in words)) + '\n    }, {\n'
        header += ',\n'.join(('        0x' + v + 'u' for v in source)) + '\n    }, {\n'
        for array in expected:
            header += '        {\n' + ',\n'.join(('            %.17g' % v for v in array)) + '\n        },\n'
        header += '    } },\n'
        records.append(dict(file=filename, sha256=hashlib.sha256(blob).hexdigest(), source=source, expected=expected))
    header += '};\nstatic unsigned char const tm_gray_profile[] = {\n'
    header += ',\n'.join(('    ' + ', '.join(('0x%02x' % v for v in gray[i:i + 12])) for i in range(0, len(gray), 12))) + '\n};\n#endif\n'
    outputs['tiff-managed.h'] = header.encode()
    outputs['tiff-managed.json'] = (json.dumps(dict(generator='tools/generate_tiff_managed_fixtures.py', oracle='tools/tiff_managed_reference.py; separate directory/raw/tag parser, rational inverse and Bradford, Decimal60 power', error_contract=dict(float_formula='64 * 2^-24', native_formula='(1 + norm*4*q/(1-norm*3*q))*(1+float)-1', inverse_norm=str(norm), q=str(q), table_absolute_formula='inverse_norm/65535', table_source_row_norm=str(source_norm), scope='Declared test allowances, not a proved opaque native arithmetic bound; black is exact zero and analytic dark values have no absolute floor'), fixtures=records), indent=2) + '\n').encode()
    return outputs
