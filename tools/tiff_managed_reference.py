"""Independent byte reader and rational colorimetric oracle.
No fixture writer, production code, or CMM is imported or executed.
"""
from fractions import Fraction as F
from decimal import Decimal, localcontext
import struct

def matmul(a, b):
    return [[sum((x * y for x, y in zip(row, col))) for col in zip(*b)] for row in a]

def inv(a):
    n = len(a)
    a = [list(row) + [F(i == j) for j in range(n)] for i, row in enumerate(a)]
    for c in range(n):
        r = next((i for i in range(c, n) if a[i][c]))
        a[c], a[r] = (a[r], a[c])
        d = a[c][c]
        a[c] = [x / d for x in a[c]]
        for r in range(n):
            if r != c:
                d = a[r][c]
                a[r] = [x - d * y for x, y in zip(a[r], a[c])]
    return [r[n:] for r in a]

def mv(m, v):
    return [sum((a * b for a, b in zip(r, v))) for r in m]

def xyz(x, y):
    return [x / y, F(1), (1 - x - y) / y]
# Derive the canonical sRGB/D65 matrix from exact chromaticities.
prim = [xyz(F(64, 100), F(33, 100)), xyz(F(30, 100), F(60, 100)), xyz(F(15, 100), F(6, 100))]
P = [list(x) for x in zip(*prim)]
white = xyz(F(3127, 10000), F(3290, 10000))
scale = mv(inv(P), white)
native = [[P[i][j] * scale[j] for j in range(3)] for i in range(3)]
# Adapt to the ICC PCS using exact stored Bradford coefficients.
B = [[F(x) for x in row.split()] for row in ['.8951 .2664 -.1614', '-.7502 1.7135 .0367', '.0389 -.0685 1.0296']]
d50 = [F('.9642'), F(1), F('.8249')]
a = mv(B, white)
b = mv(B, d50)
cat = matmul(inv(B), [[B[i][j] * b[i] / a[i] for j in range(3)] for i in range(3)])
D = inv(matmul(cat, native))

def nearest_rational(x):
    """Find adjacent binary32 values independently of the fixture writer."""
    sign = 2147483648 if x < 0 else 0
    x = abs(x)
    low, high = (0, 2139095039)

    def value(word):
        return F.from_float(struct.unpack('>f', struct.pack('>I', word))[0])
    while low < high:
        mid = (low + high + 1) // 2
        if value(mid) <= x:
            low = mid
        else:
            high = mid - 1
    if low == 2139095039:
        return sign | low
    dl = x - value(low)
    du = value(low + 1) - x
    return sign | (low if dl < du or (dl == du and low % 2 == 0) else low + 1)

def tiff(blob):
    endian = '<' if blob[:2] == b'II' else '>'
    offset = struct.unpack_from(endian + 'I', blob, 4)[0]
    n = struct.unpack_from(endian + 'H', blob, offset)[0]
    tags = {}
    sizes = {3: 2, 4: 4, 7: 1, 11: 4, 5: 8}
    for i in range(n):
        p = offset + 2 + i * 12
        tag, typ, cnt = struct.unpack_from(endian + 'HHI', blob, p)
        start = p + 8 if cnt * sizes[typ] <= 4 else struct.unpack_from(endian + 'I', blob, p + 8)[0]
        data = blob[start:start + cnt * sizes[typ]]
        tags[tag] = (typ, cnt, data)

    def vals(tag):
        typ, cnt, data = tags[tag]
        return list(struct.unpack(endian + {3: 'H', 4: 'I'}[typ] * cnt, data))
    w, h = (vals(256)[0], vals(257)[0])
    ch = vals(277)[0]
    raw = b''.join((blob[o:o + n] for o, n in zip(vals(273), vals(279))))
    bits = vals(258)[0]
    photo = vals(262)[0]
    orientation = vals(274)[0]
    if bits == 16:
        assert ch == 1 and photo in (0, 1) and vals(339) == [1]
        words = list(struct.unpack(endian + 'H' * (len(raw) // 2), raw))
        # Apply polarity to the exact normalized integer before binary32
        # rounding. Do not reuse the serializer's floating-point operations.
        samples = [F(x if photo == 1 else 65535-x, 65535) for x in words]
        triples = [[struct.unpack('>f', struct.pack('>I', nearest_rational(x)))[0]] * 3 for x in samples]
    else:
        assert bits == 32
        words = list(struct.unpack(endian + 'I' * (len(raw) // 4), raw))
        floats = [struct.unpack('>f', struct.pack('>I', x))[0] for x in words]
        triples = [floats[i:i + 3] for i in range(0, len(floats), 3)] if ch == 3 else [[x if photo == 1 else struct.unpack('>f', struct.pack('>I', nearest_rational(F(1) - F.from_float(x))))[0]] * 3 for x in floats]
    mapped = [None] * (w * h)
    for y in range(h):
        for x in range(w):
            mapped[(h - 1 - y if orientation in [3, 4] else y) * w + (w - 1 - x if orientation in [2, 3] else x)] = triples[y * w + x]
    return dict(width=w, height=h, photo=photo, bits=bits, words=words, triples=mapped, icc=tags.get(34675, (0, 0, None))[2])

def tags(p):
    out = {}
    for i in range(struct.unpack_from('>I', p, 128)[0]):
        sig, offset, size = struct.unpack_from('>4sII', p, 132 + 12 * i)
        out[sig] = p[offset:offset + size]
    return out

def power(x, g):
    assert x >= 0
    with localcontext() as c:
        c.prec = 60
        xx = Decimal(x.numerator) / Decimal(x.denominator)
        gg = Decimal(g.numerator) / Decimal(g.denominator)
        return F(xx ** gg)

def curve(tag, x, clamp=False):
    x = F(x)
    if clamp:
        x = min(F(1), max(F(0), x))
    if tag[:4] == b'para':
        typ = struct.unpack_from('>H', tag, 8)[0]
        p = [F(v, 65536) for v in struct.unpack('>' + 'i' * ((len(tag) - 12) // 4), tag[12:])]
        if typ == 0:
            return power(max(x, F(0)), p[0])
        assert typ == 4
        g, a, b, c, d, e, f = p
        return power(a * x + b, g) + e if x >= d else c * x + f
    assert tag[:4] == b'curv'
    n = struct.unpack_from('>I', tag, 8)[0]
    if n == 0:
        return x
    if n == 1:
        return power(max(x, F(0)), F(struct.unpack_from('>H', tag, 12)[0], 256))
    entries = [F(v, 65535) for v in struct.unpack('>' + 'H' * n, tag[12:12 + 2 * n])]
    x = min(F(1), max(F(0), x)) * (n - 1)
    i = min(int(x), n - 2)
    return entries[i] * (i + 1 - x) + entries[i + 1] * (x - i)

def expected(item, clamp=False):
    """Evaluate stored ICC tags; clamp only for a stated device-domain model."""
    p = item['icc']
    t = tags(p)
    out = []
    for rgb in item['triples']:
        if p[16:20] == b'GRAY':
            v = curve(t[b'kTRC'], rgb[0], clamp)
            out.extend([float(v)] * 3)
        else:
            lin = [curve(t[k], v, clamp) for k, v in zip([b'rTRC', b'gTRC', b'bTRC'], rgb)]
            M = [list(r) for r in zip(*[[F(v, 65536) for v in struct.unpack('>iii', t[k][8:20])] for k in [b'rXYZ', b'gXYZ', b'bXYZ']])]
            out.extend((float(v) for v in mv(D, mv(M, lin))))
    return out
