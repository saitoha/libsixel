#!/usr/bin/env python3
"""Generate output ICC and loader regression fixtures independently."""
from pathlib import Path
import struct
from zlib import compress
import generate_color_fixtures as source

root = Path(__file__).resolve().parents[1]
profile = bytearray(source.lut_profile(True))
for n in range(struct.unpack_from('>I', profile, 128)[0]):
    offset = 132 + n * 12
    if profile[offset:offset+4] in (b'A2B0', b'B2A0'):
        profile[offset+3] = ord('1')
(root / 'tests/data/color-numeric/relative-paired-lut.icc').write_bytes(profile)

# Non-inverse relative LUTs for fixed-palette feedback. Forward PCS is
# D50 times (0.5 + R/2), while reverse device RGB depends only on PCS Y.
def register_lut(grid, points):
    header = b'mft2' + bytes(4) + bytes([3, 3, grid, 0])
    header += b''.join(source.fixed(source.D(i == j))
                       for i in range(3) for j in range(3))
    ramps = struct.pack('>HH6H', 2, 2, 0, 65535, 0, 65535, 0, 65535)
    return header + ramps + b''.join(struct.pack('>3H', *p) for p in points) + ramps[4:]


forward = [tuple(round(v * (1 + r) * 16384) for v in source.D50)
           for r in range(2) for _ in range(4)]
reverse = [tuple(round(min(y, 1) * v * 65535)
                 for v in (source.D(1), source.D('.30'), source.D('.05')))
           for _ in range(3) for y in range(3) for _ in range(3)]
profile = source.pack_profile([(b'A2B1', register_lut(2, forward)),
                               (b'B2A1', register_lut(3, reverse))])
(root / 'tests/data/color-numeric/register-feedback-lut.icc').write_bytes(profile)


def matrix_mpe(matrix, bias):
    element = b'matf' + bytes(4) + struct.pack('>HH12f', 3, 3, *matrix, *bias)
    return (b'mpet' + bytes(4)
            + struct.pack('>HHIII', 3, 3, 1, 24, len(element)) + element)


# Same neutral-axis oracle as the mft2 pair, with physical floating PCS.
half_white = [v / 2 for v in source.D50]
forward = matrix_mpe([value for v in half_white for value in (v, 0, 0)],
                     half_white)
reverse = matrix_mpe([0, 1, 0, 0, .30, 0, 0, .05, 0], [0, 0, 0])
profile = source.pack_profile([(b'D2B1', forward), (b'B2D1', reverse)],
                              name='Relative MPE wire feedback')
(root / 'tests/data/color-numeric/register-feedback-mpet.icc').write_bytes(profile)


def alpha_replay_apng(icc):
    def frame_control(sequence):
        return struct.pack('>IIIIIHHBB', sequence, 1, 1, 0, 0, 1, 100, 0, 0)

    chunks = [
        (b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0)),
        (b'acTL', struct.pack('>II', 2, 2)),
        (b'iCCP', b'icc\0\0' + compress(icc)),
        (b'fcTL', frame_control(0)),
        (b'IDAT', compress(bytes([0, 255, 0, 0, 128]))),
        (b'fcTL', frame_control(1)),
        (b'fdAT', struct.pack('>I', 2) + compress(bytes([0, 255, 0, 0, 64]))),
        (b'IEND', b''),
    ]
    return b'\x89PNG\r\n\x1a\n' + b''.join(source.chunk(*c) for c in chunks)


(root / 'tests/data/color-numeric/red-alpha-p3.apng').write_bytes(
    alpha_replay_apng(source.icc_profile()))

# A nonzero black table exposes triple-counting of the table baseline.
original = source.icc_profile()
tags = []
columns = []
for n in range(struct.unpack_from('>I', original, 128)[0]):
    key, offset, length = struct.unpack_from('>4sII', original, 132+n*12)
    data = original[offset:offset+length]
    if key in (b'rTRC', b'gTRC', b'bTRC'):
        data = b'curv' + bytes(4) + struct.pack('>IHH', 2, 8192, 57344)
    if key in (b'rXYZ', b'gXYZ', b'bXYZ'):
        columns.append([source.D(x)/65536 for x in struct.unpack_from('>3i', data, 8)])
    tags.append((key, data))
(root / 'tests/data/color-numeric/black-offset.icc').write_bytes(source.pack_profile(tags))
srgb_d50 = list(map(list, zip(*(source.adapt(col) for col in zip(*source.SRGB)))))
p3_d50 = list(map(list, zip(*columns)))
codes = [(0, 0, 0), (10, 50, 90), (100, 100, 100)]
lines = ['/* Independent stored ICC fields and Decimal D50 arithmetic. */',
         'static double const icco_black_expected[][3] = {']
for rgb in codes:
    encoded = [source.D((q*255+50)//100)/255 for q in rgb]
    linear = [(8192 + 49152*v)/65535 for v in encoded]
    xyz = source.apply(p3_d50, linear)
    result = source.apply(source.inverse(srgb_d50), xyz)
    lines.append('    {' + ', '.join(format(float(v), '.17g') for v in result) + '},')
lines += ['};', '']

# D2B accepts (.848,.848,0); projecting 85% to RGB888 raises both
# channels to 217/255, whose sum makes the float32 PCS X overflow.
forward = matrix_mpe([2e38, 2e38, -2e38, 0, 0, 0, 0, 0, 0],
                     [0, .2, .15])
reverse = matrix_mpe([0, 1, 0, 0, 1, 0, 0, 0, 0], [0, 0, 0])
rounding = source.pack_profile([(b'D2B1', forward), (b'B2D1', reverse)],
                               name='Rounded ICC PCS overflow')
lines += ['/* Relative MPE with a rounding-dependent PCS domain failure. */',
          'static unsigned char const icco_rounding_profile[] = {']
for i in range(0, len(rounding), 12):
    lines.append('    ' + ', '.join(f'0x{v:02x}' for v in rounding[i:i+12]) + ',')
lines += ['};', '']
(root / 'tests/data/color-numeric/icc-output.h').write_text('\n'.join(lines))

# Diagonal D50 profiles with exact affine input curves. In normalized PCS,
# a black remap from 1/4 to 1/2 changes the source curve into the reference
# curve. Thus the reference PNG can keep the *same* original RGB/alpha words
# and differ only in its embedded ICC, even when composed against bKGD.
def output_bpc_png(icc):
    chunks = [
        (b'IHDR', struct.pack('>IIBBBBB', 6, 1, 16, 6, 0, 0, 0)),
        (b'iCCP', b'affine\0\0' + compress(icc)),
        (b'bKGD', struct.pack('>3H', 4096, 4096, 4096)),
    ]
    scanline = bytes(1)
    for value, alpha in zip([0, 8192, 16384, 32768, 49152, 65535],
                            [65535, 65535, 16384, 32768, 49151, 65535]):
        scanline += struct.pack('>4H', value, value, value, alpha)
    chunks.extend([(b'IDAT', compress(scanline)), (b'IEND', b'')])
    return b'\x89PNG\r\n\x1a\n' + b''.join(source.chunk(*c) for c in chunks)


directory = root / 'tests/data/color-numeric'
output_profile = source.affine_profile(32768)
(directory / 'output-bpc.icc').write_bytes(output_profile)
(directory / 'output-bpc-rgba16.png').write_bytes(
    output_bpc_png(source.affine_profile(16384)))
(directory / 'output-bpc-rgba16-reference.png').write_bytes(
    output_bpc_png(output_profile))
