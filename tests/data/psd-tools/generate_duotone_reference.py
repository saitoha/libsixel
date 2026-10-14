# SPDX-License-Identifier: MIT
"""Regenerate the fixed Duotone Lab-table PPM oracle without a loader/CMM.

The source is the pinned 4x4 raw, eight-bit psd-tools fixture. Matrix and
Bradford arithmetic are rational; only the final gamma-2.2 byte encoding
uses floating point. The default PPM is a bounded legacy-gamma reference; --srgb produces
a display-sRGB sample oracle without an encoder quantization boundary.
The separate frame tests protect extended values before this narrowing.
"""
import argparse
from pathlib import Path
import struct
from fractions import Fraction as F

def mul(a,b):
 return [[sum(x*y for x,y in zip(row,col)) for col in zip(*b)] for row in a]
def inv(m):
 a=[r[:]+[F(i==j) for j in range(3)] for i,r in enumerate(m)]
 for i in range(3):
  v=a[i][i];a[i]=[x/v for x in a[i]]
  for j in range(3):
   if j!=i:
    v=a[j][i];a[j]=[x-v*y for x,y in zip(a[j],a[i])]
 return [r[3:] for r in a]
def col(x):return [[F(v)] for v in x]
white65=col([F(3127,3290),1,F(3583,3290)])
white50=col(['0.9642',1,'0.8249'])
pairs=[('0.64','0.33'),('0.30','0.60'),('0.15','0.06')]
primary=list(map(list,zip(*[(F(x)/F(y),F(1),(1-F(x)-F(y))/F(y)) for x,y in pairs])))
s=mul(inv(primary),white65)
rgb_xyz=[[primary[i][j]*s[j][0] for j in range(3)] for i in range(3)]
b=[[F(x) for x in r.split()] for r in ['.8951 .2664 -.1614','-.7502 1.7135 .0367','.0389 -.0685 1.0296']]
s50=mul(b,white50);s65=mul(b,white65)
adapt=mul(mul(inv(b),[[s65[i][0]/s50[i][0] if i==j else F(0) for j in range(3)] for i in range(3)]),b)


def generate_duotone_expected(source: Path, destination: Path, srgb: bool = False) -> None:
    data = source.read_bytes()
    assert data[:6] == b"8BPS\x00\x01"
    assert struct.unpack_from(">HIIHH", data, 12) == (1, 4, 4, 8, 8)
    offset = 30 + struct.unpack_from(">I", data, 26)[0]
    resource_end = offset + 4 + struct.unpack_from(">I", data, offset)[0]
    offset += 4
    table = None
    while offset < resource_end:
        assert data[offset:offset + 4] == b"8BIM"
        resource_id = struct.unpack_from(">H", data, offset + 4)[0]
        offset += 6
        name_length = data[offset] + 1
        offset += name_length + name_length % 2
        length = struct.unpack_from(">I", data, offset)[0]
        offset += 4
        if resource_id == 1066:
            version, count = struct.unpack_from(">HH", data, offset)
            assert version == 1 and 1 <= count <= 4
            pos = offset + 4 + count * 10
            assert struct.unpack_from(">H", data, pos)[0] == 256
            assert length == 6 + count * 10 + 768
            table = data[pos + 2:pos + 2 + 768]
        offset += length + length % 2
    assert table is not None
    offset += 4 + struct.unpack_from(">I", data, offset)[0]
    assert struct.unpack_from(">H", data, offset)[0] == 0
    samples = data[offset + 2:]
    assert len(samples) == 16
    matrix = mul(inv(rgb_xyz), adapt)
    pixels = bytearray()
    for value in samples:
        l, a, bb = table[3 * value:3 * value + 3]
        l, a, bb = F(l * 100, 255), F(a - 128), F(bb - 128)
        y = (l + 16) / 116
        coordinates = [y + a / 500, y, y - bb / 200]
        xyz = []
        for i, t in enumerate(coordinates):
            v = t ** 3 if t > F(6, 29) else 3 * F(6, 29) ** 2 * (t - F(4, 29))
            xyz.append([white50[i][0] * v])
        for channel in mul(matrix, xyz):
            bounded = min(1.0, max(0.0, float(channel[0])))
            encoded = bounded ** (1 / 2.2)
            if srgb:
                encoded = (12.92 * bounded if bounded <= .0031308
                           else 1.055 * bounded ** (1 / 2.4) - .055)
            pixels.append(int(255 * encoded + 0.5))
    destination.write_bytes(b"P6\n4 4\n255\n" + pixels)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--srgb", action="store_true")
    args = parser.parse_args()
    generate_duotone_expected(args.source, args.destination, args.srgb)
