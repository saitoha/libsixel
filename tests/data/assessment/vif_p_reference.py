#!/usr/bin/env python3
"""Regenerate VIF-P specimens and independent expectations offline.

Needs NumPy; never invoked by the TAP suite. The source is the existing
64x64 snake BMP. Direct two-dimensional Gaussian reductions implement the
scalar pixel-domain model described by Sheikh and Bovik:
https://live.ece.utexas.edu/research/quality/VIF.htm
Neither lsqa nor its separable C filter supplies an expected value.

Run with --write to save deterministic PPM/PGM specimens beside this file.
Otherwise print expectations and conservative input-rounding error bounds.
"""

import argparse
import json
from pathlib import Path
import struct

import numpy as np
from numpy.lib.stride_tricks import sliding_window_view


def snake():
    """Read the committed, uncompressed 24-bit BMP without a CMS/loader."""
    data = (Path(__file__).parent.parent / 'inputs/snake_64.bmp').read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, depth, compression = struct.unpack_from(
        '<iiHHI', data, 18)
    assert (width, height, planes, depth, compression) == (64, 64, 1, 24, 0)
    return np.frombuffer(data, dtype=np.uint8, offset=offset).reshape(
        height, width, 3)[::-1, :, ::-1].copy()


def reference(rgb_a, rgb_b):
    """Return pooled information and its ratio, with input-error bounds.

    Ideal byte samples and BT.709 coefficients define the reference signal.
    A 4e-7 relative input budget covers byte normalization, float32 storage,
    coefficient rounding and the three products/two sums of luma conversion.
    Normalized positive filters cannot enlarge that absolute perturbation.
    Variance/covariance bounds propagate it through gain, residual noise,
    log1p and the final information ratio. This bound is for the textured,
    positive-covariance fixtures, not for the exact zero/unavailable cases.
    """
    a = rgb_a.astype(float) @ np.array([.2126, .7152, .0722])
    b = rgb_b.astype(float) @ np.array([.2126, .7152, .0722])
    da, db = float(a.max()) * 4e-7, float(b.max()) * 4e-7
    numerator, denominator, numerator_error, denominator_error = 0., 0., 0., 0.
    scales = []
    for size in [17, 9, 5, 3]:
        axis = np.arange(size) - size // 2
        window = np.exp(-(axis[:, None] ** 2 + axis[None, :] ** 2)
                        / (2 * (size / 5) ** 2))
        window /= window.sum()

        def smooth(image):
            return np.einsum('ijkl,kl->ij',
                             sliding_window_view(image, (size, size)), window)

        if size != 17:
            a, b = smooth(a)[::2, ::2], smooth(b)[::2, ::2]
        ma, mb = smooth(a), smooth(b)
        va = np.maximum(0, smooth(a * a) - ma * ma)
        vb = np.maximum(0, smooth(b * b) - mb * mb)
        covariance = smooth(a * b) - ma * mb
        gain = covariance / (va + 1e-10)
        residual = vb - gain * covariance
        # These specimens stay away from flat/negative-correlation branches,
        # allowing a conservative differential bound with finite margins.
        assert np.all(va > 1e-10) and np.all(vb > 1e-10)
        assert np.all(covariance > 0)
        dva = 2 * np.sqrt(va) * da + da * da
        dvb = 2 * np.sqrt(vb) * db + db * db
        dc = np.sqrt(va) * db + np.sqrt(vb) * da + da * db
        assert np.all(va > dva) and np.all(covariance > dc)
        dg = (dc + abs(gain) * dva) / (va + 1e-10 - dva)
        dr = dvb + abs(gain) * dc + abs(covariance) * dg + dg * dc
        residual = np.maximum(residual, 1e-10)
        signal = gain * gain * va
        ds = (2 * abs(gain) * dg + dg * dg) * (va + dva) + gain * gain * dva
        q = signal / (residual + 2)
        assert np.all(residual + 2 > dr)
        dq = (ds + q * dr) / (residual + 2 - dr)
        assert np.all(1 + q > dq)
        ne = float(np.sum(dq / (1 + q - dq)))
        de = float(np.sum(dva / (2 + va - dva)))
        n = float(np.log1p(q).sum())
        d = float(np.log1p(va / 2).sum())
        numerator += n
        denominator += d
        numerator_error += ne
        denominator_error += de
        scales.append({'numerator': n, 'denominator': d})
    score = numerator / denominator
    error = (numerator_error + score * denominator_error) / (
        denominator - denominator_error)
    return {'score': score, 'input_error_bound': error, 'scales': scales}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    a = snake()
    # A reproducible mixture of blur and bounded noise tests all four scales.
    padded = np.pad(a.astype(int), ((1, 1), (1, 1), (0, 0)), mode='edge')
    y, x = np.indices((64, 64))
    b = np.clip((padded[1:-1, 1:-1] * 4 + padded[:-2, 1:-1]
                 + padded[2:, 1:-1] + padded[1:-1, :-2]
                 + padded[1:-1, 2:]) // 8
                + ((3 * x + 7 * y) % 17 - 8)[:, :, None], 0, 255).astype('u1')
    dim = (64 + a // 2).astype('u1')
    specimens = {
        'vif-p-distorted.ppm': b,
        'vif-p-odd-reference.ppm': a[:61, :63],
        'vif-p-odd-distorted.ppm': b[:61, :63],
        'vif-p-dim.ppm': dim,
        'vif-p-minimum.ppm': a[:41, :41],
        'vif-p-narrow.ppm': a[:, :40],
        'vif-p-short.ppm': a[:40],
        'vif-p-flat.pgm': np.full((64, 64), 128, dtype='u1'),
        'vif-p-inverse.ppm': 255 - a,
    }
    if args.write:
        for name, pixels in specimens.items():
            height, width = pixels.shape[:2]
            magic = 'P6' if pixels.ndim == 3 else 'P5'
            (Path(__file__).parent / name).write_bytes(
                f'{magic}\n{width} {height}\n255\n'.encode() + pixels.tobytes())
    values = {
        'even': reference(a, b),
        'odd': reference(a[:61, :63], b[:61, :63]),
        'reverse': reference(b, a),
        'contrast': reference(dim, a),
    }
    print(json.dumps(values, indent=2))


if __name__ == '__main__':
    main()
