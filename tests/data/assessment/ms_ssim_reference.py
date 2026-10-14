#!/usr/bin/env python3
"""Regenerate independent scalar expectations; not run by the TAP suite.

Needs NumPy. Follows Wang's msssim.m / ssim_index_new.m, product mode:
https://ece.uwaterloo.ca/~z70wang/research/iwssim/msssim.zip
Uses direct 2-D window reduction, independently of the C separable moments.
Inputs are ideal binary-fraction gray levels, except 3/255 in the flat case.
The C observation includes float32 luma conversion; its budget is in
docs/quality/metrics.md. No generated value is taken from lsqa.
"""

import json

import numpy as np
from numpy.lib.stride_tricks import sliding_window_view


def reference(a, b):
    axis = np.arange(11) - 5
    window = np.exp(-(axis[:, None] ** 2 + axis[None, :] ** 2) / 4.5)
    window /= window.sum()
    weights = [.0448, .2856, .3001, .2363, .1333]
    factors = []
    errors = []
    # Input storage plus three weighted float products and two sums. This
    # bounds sample perturbations, not the difference observed on one host.
    da, db = np.max(abs(a)) * 4e-7, np.max(abs(b)) * 4e-7

    def convolve(image):
        return np.einsum('ijkl,kl->ij',
                         sliding_window_view(image, (11, 11)), window)

    def downsample(image):
        height, width = image.shape
        padded = np.pad(image, ((0, height % 2), (0, width % 2)),
                        mode='symmetric')
        return (padded[::2, ::2] + padded[1::2, ::2] +
                padded[::2, 1::2] + padded[1::2, 1::2]) / 4

    for level in range(5):
        mx, my = convolve(a), convolve(b)
        vx = np.maximum(0, convolve(a * a) - mx * mx)
        vy = np.maximum(0, convolve(b * b) - my * my)
        cs = (2 * (convolve(a * b) - mx * my) + .0009) / (
            convolve(a * a) - mx * mx + convolve(b * b) - my * my + .0009)
        luminance = (2 * mx * my + .0001) / (mx * mx + my * my + .0001)
        # |delta variance| <= 2 sigma delta + delta^2; Cauchy-Schwarz
        # bounds covariance perturbation by sigma_x db + sigma_y da + da db.
        denominator = vx + vy + .0009
        de = 2 * np.sqrt(vx) * da + da * da + 2 * np.sqrt(vy) * db + db * db
        ne = 2 * (np.sqrt(vx) * db + np.sqrt(vy) * da + da * db)
        error = (ne + abs(cs) * de) / (denominator - de)
        if level == 4:
            denominator = mx * mx + my * my + .0001
            de = 2 * abs(mx) * da + da * da + 2 * abs(my) * db + db * db
            ne = 2 * (abs(mx) * db + abs(my) * da + da * db)
            le = (ne + abs(luminance) * de) / (denominator - de)
            error = abs(luminance) * error + abs(cs) * le + error * le
        factors.append(float(np.mean(cs * luminance if level == 4 else cs)))
        errors.append(float(np.mean(error)))
        a, b = downsample(a), downsample(b)
    f, e = np.array(factors), np.array(errors)
    score = float(np.prod(f ** weights))
    budget = max(np.prod((f + e) ** weights) - score,
                 score - np.prod((f - e) ** weights))
    return {'factors': factors,
            'score': score, 'input_error_bound': float(budget)}


def fixtures():
    result = {}
    for width, height in [(192, 176), (177, 181)]:
        y, x = np.indices((height, width))
        values = (17 * x + 29 * y + (x * y) % 37) % 1024
        a = values / 1024
        b = (3 * values + ((x // 17 + 3 * (y // 13)) % 64) * 8) / 4096
        result[f'{width}x{height}'] = reference(a, b)
    result['black'] = reference(np.zeros((176, 176)),
                                np.full((176, 176), 3 / 255))
    result['checker'] = reference(np.full((176, 176), .5),
        np.indices((176, 176)).sum(axis=0) % 2 * .5 + .25)
    return result


if __name__ == '__main__':
    print(json.dumps(fixtures(), indent=2))
