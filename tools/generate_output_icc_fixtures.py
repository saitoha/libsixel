#!/usr/bin/env python3
"""Generate a relative-intent constant RGB ICC, independent of libsixel."""
from pathlib import Path
import struct
import generate_color_fixtures as source

root = Path(__file__).resolve().parents[1]
profile = bytearray(source.lut_profile(True))
for n in range(struct.unpack_from('>I', profile, 128)[0]):
    offset = 132 + n * 12
    if profile[offset:offset+4] in (b'A2B0', b'B2A0'):
        profile[offset+3] = ord('1')
(root / 'tests/data/color-numeric/relative-paired-lut.icc').write_bytes(profile)

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
(root / 'tests/data/color-numeric/icc-output.h').write_text('\n'.join(lines))

# Diagonal D50 profiles with exact affine input curves. In normalized PCS,
# a black remap from 1/4 to 1/2 changes the source curve into the reference
# curve. Thus the reference PNG can keep the *same* original RGB/alpha words
# and differ only in its embedded ICC, even when composed against bKGD.
def output_bpc_png(icc):
    from zlib import compress
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
