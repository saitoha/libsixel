#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the figures of the transfer-function and primaries guides.

The figures of docs/concepts/colorspace/transfer-functions.md and
docs/concepts/colorspace/primaries-and-white-points.md are hand-written SVG,
with embedded PNG images where a picture is needed, so their bytes do not
depend on a plotting library.  Run without arguments to regenerate them in
docs/concepts/colorspace/figures, or with --check to verify that the
committed files are current.  NumPy and Pillow are required; the SVG helpers
are shared with tools/plot_visual_perception_figures.py.
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import math

import numpy as np
from PIL import Image

from plot_visual_perception_figures import (GRID, INK, MUTED, ROOT, srgb_decode,
                                            srgb_encode, svg_document, svg_text)

OUT = ROOT / 'docs/concepts/colorspace/figures'
ACCENT = '#2f5fb3'
WARM = '#c0392b'
GREEN = '#2e8b57'
ORANGE = '#d9822b'
PURPLE = '#7b4fa0'
GRAY = '#7a8793'

# CIE 1931 2-degree standard observer colour-matching functions, 380-780 nm
# in 5 nm steps (CIE 015:2018), as tabulated by the Colour & Vision Research
# Laboratory (CVRL).
CIE1931_CMF = (
    (380, 0.001368, 3.9e-05, 0.00645),
    (385, 0.002236, 6.4e-05, 0.01055),
    (390, 0.004243, 0.00012, 0.02005),
    (395, 0.00765, 0.000217, 0.03621),
    (400, 0.01431, 0.000396, 0.06785),
    (405, 0.02319, 0.00064, 0.1102),
    (410, 0.04351, 0.00121, 0.2074),
    (415, 0.07763, 0.00218, 0.3713),
    (420, 0.13438, 0.004, 0.6456),
    (425, 0.21477, 0.0073, 1.03905),
    (430, 0.2839, 0.0116, 1.3856),
    (435, 0.3285, 0.01684, 1.62296),
    (440, 0.34828, 0.023, 1.74706),
    (445, 0.34806, 0.0298, 1.7826),
    (450, 0.3362, 0.038, 1.77211),
    (455, 0.3187, 0.048, 1.7441),
    (460, 0.2908, 0.06, 1.6692),
    (465, 0.2511, 0.0739, 1.5281),
    (470, 0.19536, 0.09098, 1.28764),
    (475, 0.1421, 0.1126, 1.0419),
    (480, 0.09564, 0.13902, 0.81295),
    (485, 0.05795, 0.1693, 0.6162),
    (490, 0.03201, 0.20802, 0.46518),
    (495, 0.0147, 0.2586, 0.3533),
    (500, 0.0049, 0.323, 0.272),
    (505, 0.0024, 0.4073, 0.2123),
    (510, 0.0093, 0.503, 0.1582),
    (515, 0.0291, 0.6082, 0.1117),
    (520, 0.06327, 0.71, 0.07825),
    (525, 0.1096, 0.7932, 0.05725),
    (530, 0.1655, 0.862, 0.04216),
    (535, 0.22575, 0.91485, 0.02984),
    (540, 0.2904, 0.954, 0.0203),
    (545, 0.3597, 0.9803, 0.0134),
    (550, 0.43345, 0.99495, 0.00875),
    (555, 0.51205, 1, 0.00575),
    (560, 0.5945, 0.995, 0.0039),
    (565, 0.6784, 0.9786, 0.00275),
    (570, 0.7621, 0.952, 0.0021),
    (575, 0.8425, 0.9154, 0.0018),
    (580, 0.9163, 0.87, 0.00165),
    (585, 0.9786, 0.8163, 0.0014),
    (590, 1.0263, 0.757, 0.0011),
    (595, 1.0567, 0.6949, 0.001),
    (600, 1.0622, 0.631, 0.0008),
    (605, 1.0456, 0.5668, 0.0006),
    (610, 1.0026, 0.503, 0.00034),
    (615, 0.9384, 0.4412, 0.00024),
    (620, 0.85445, 0.381, 0.00019),
    (625, 0.7514, 0.321, 0.0001),
    (630, 0.6424, 0.265, 5e-05),
    (635, 0.5419, 0.217, 3e-05),
    (640, 0.4479, 0.175, 2e-05),
    (645, 0.3608, 0.1382, 1e-05),
    (650, 0.2835, 0.107, 0),
    (655, 0.2187, 0.0816, 0),
    (660, 0.1649, 0.061, 0),
    (665, 0.1212, 0.04458, 0),
    (670, 0.0874, 0.032, 0),
    (675, 0.0636, 0.0232, 0),
    (680, 0.04677, 0.017, 0),
    (685, 0.0329, 0.01192, 0),
    (690, 0.0227, 0.00821, 0),
    (695, 0.01584, 0.005723, 0),
    (700, 0.0113592, 0.004102, 0),
    (705, 0.00811092, 0.002929, 0),
    (710, 0.00579035, 0.002091, 0),
    (715, 0.00410946, 0.001484, 0),
    (720, 0.00289933, 0.001047, 0),
    (725, 0.00204919, 0.00074, 0),
    (730, 0.00143997, 0.00052, 0),
    (735, 0.000999949, 0.0003611, 0),
    (740, 0.000690079, 0.0002492, 0),
    (745, 0.000476021, 0.0001719, 0),
    (750, 0.000332301, 0.00012, 0),
    (755, 0.000234826, 8.48e-05, 0),
    (760, 0.000166151, 6e-05, 0),
    (765, 0.000117413, 4.24e-05, 0),
    (770, 8.30753e-05, 3e-05, 0),
    (775, 5.87065e-05, 2.12e-05, 0),
    (780, 4.15099e-05, 1.499e-05, 0),
)

# Chromaticities of the color spaces drawn in the guides.  sRGB, Display P3,
# Adobe RGB (1998), ProPhoto RGB and BT.2020 follow CSS Color 4; SMPTE-C is
# SMPTE RP 145, EBU is EBU Tech 3213, NTSC is the 1953 FCC system and DCI-P3
# is SMPTE RP 431-2.
WHITES = {
    'D65': (0.3127, 0.3290),
    'D50': (0.3457, 0.3585),
    'C': (0.3101, 0.3162),
    'DCI': (0.314, 0.351),
    'E': (1 / 3, 1 / 3),
}
SPACES = {
    'NTSC 1953': {'primaries': ((0.67, 0.33), (0.21, 0.71), (0.14, 0.08)), 'white': 'C',
                  'color': GRAY, 'dash': '2 3'},
    'SMPTE-C': {'primaries': ((0.630, 0.340), (0.310, 0.595), (0.155, 0.070)), 'white': 'D65',
                'color': ORANGE, 'dash': ''},
    'sRGB / BT.709': {'primaries': ((0.64, 0.33), (0.30, 0.60), (0.15, 0.06)), 'white': 'D65',
                      'color': INK, 'dash': ''},
    'Adobe RGB (1998)': {'primaries': ((0.64, 0.33), (0.21, 0.71), (0.15, 0.06)), 'white': 'D65',
                         'color': GREEN, 'dash': '6 3'},
    'Display P3': {'primaries': ((0.680, 0.320), (0.265, 0.690), (0.150, 0.060)), 'white': 'D65',
                   'color': WARM, 'dash': ''},
    'BT.2020': {'primaries': ((0.708, 0.292), (0.170, 0.797), (0.131, 0.046)), 'white': 'D65',
                'color': ACCENT, 'dash': ''},
    'ProPhoto RGB': {'primaries': ((0.734699, 0.265301), (0.159597, 0.840403),
                                   (0.036598, 0.000105)), 'white': 'D50',
                     'color': PURPLE, 'dash': '6 3'},
}


# --------------------------------------------------------------------------
# Shared helpers

def fmt(value):
    text = f'{value:.2f}'.rstrip('0').rstrip('.')
    return '0' if text == '-0' else text


def png_data_uri(encoded_rgb):
    pixels = np.round(np.clip(encoded_rgb, 0, 1) * 255).astype(np.uint8)
    buffer = io.BytesIO()
    Image.fromarray(pixels, 'RGB').save(buffer, format='PNG', optimize=True)
    return 'data:image/png;base64,' + base64.b64encode(buffer.getvalue()).decode('ascii')


def image(x, y, width, height, encoded_rgb, smooth=False):
    rendering = '' if smooth else ' image-rendering="optimizeSpeed"'
    return (f'<image x="{fmt(x)}" y="{fmt(y)}" width="{fmt(width)}" height="{fmt(height)}" '
            f'preserveAspectRatio="none"{rendering} href="{png_data_uri(encoded_rgb)}"/>')


def hexrgb(encoded):
    code = np.round(np.clip(encoded, 0, 1) * 255).astype(int)
    return '#%02x%02x%02x' % tuple(code)


def rect(x, y, width, height, fill, stroke='none', extra=''):
    return (f'<rect x="{fmt(x)}" y="{fmt(y)}" width="{fmt(width)}" height="{fmt(height)}" '
            f'fill="{fill}" stroke="{stroke}"{extra}/>')


def line(x1, y1, x2, y2, color=INK, width=1.5, dash=''):
    dashing = f' stroke-dasharray="{dash}"' if dash else ''
    return (f'<path d="M {x1:.1f} {y1:.1f} L {x2:.1f} {y2:.1f}" stroke="{color}" '
            f'stroke-width="{width}"{dashing} fill="none"/>')


def polyline(points, color=INK, width=2.5, dash='', fill='none', close=False):
    dashing = f' stroke-dasharray="{dash}"' if dash else ''
    tag = 'polygon' if close else 'polyline'
    pts = ' '.join(f'{x:.1f},{y:.1f}' for x, y in points)
    return (f'<{tag} points="{pts}" fill="{fill}" stroke="{color}" stroke-width="{width}"'
            f'{dashing} stroke-linejoin="round"/>')


def rotated_label(x, y, value, size=14):
    return svg_text(x, y, value, size, INK, 400, 'middle').replace(
        '<text ', f'<text transform="rotate(-90 {fmt(x)} {fmt(y)})" ', 1)


class Axes:
    """A plot area with linear or logarithmic axes."""

    def __init__(self, x, y, width, height, xrange, yrange, xlog=False, ylog=False):
        self.x, self.y, self.w, self.h = x, y, width, height
        self.xrange, self.yrange, self.xlog, self.ylog = xrange, yrange, xlog, ylog

    def _t(self, value, rng, log):
        lo, hi = rng
        if log:
            return (math.log10(value) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
        return (value - lo) / (hi - lo)

    def px(self, value):
        return self.x + self._t(value, self.xrange, self.xlog) * self.w

    def py(self, value):
        return self.y + self.h - self._t(value, self.yrange, self.ylog) * self.h

    def frame(self, xticks, yticks, xlabel, ylabel, xfmt=fmt, yfmt=fmt):
        body = [rect(self.x, self.y, self.w, self.h, '#ffffff', GRID)]
        for t in xticks:
            body.append(line(self.px(t), self.y, self.px(t), self.y + self.h, GRID, 1))
            body.append(svg_text(self.px(t), self.y + self.h + 18, xfmt(t), 12, MUTED, 400,
                                 'middle'))
        for t in yticks:
            body.append(line(self.x, self.py(t), self.x + self.w, self.py(t), GRID, 1))
            body.append(svg_text(self.x - 7, self.py(t) + 4, yfmt(t), 12, MUTED, 400, 'end'))
        body.append(svg_text(self.x + self.w / 2, self.y + self.h + 40, xlabel, 13, INK, 400,
                             'middle'))
        body.append(rotated_label(self.x - 46, self.y + self.h / 2, ylabel, 13))
        return body

    def curve(self, xs, ys, color, width=2.5, dash=''):
        pts = [(self.px(a), self.py(b)) for a, b in zip(xs, ys)]
        return polyline(pts, color, width, dash)


def legend(x, y, entries, size=13, step=21):
    body = []
    for i, (label, color, dash) in enumerate(entries):
        yy = y + i * step
        body.append(line(x, yy - 4, x + 26, yy - 4, color, 3, dash))
        body.append(svg_text(x + 34, yy, label, size, INK))
    return body


def compose(panels, columns, title, description, gap=28, margin=18):
    """Arrange (width, height, draw) panels in a grid."""
    rows = [panels[i:i + columns] for i in range(0, len(panels), columns)]
    width = 2 * margin + max(sum(p[0] for p in row) + gap * (len(row) - 1) for row in rows)
    body, y = [], margin
    for row in rows:
        x = margin
        for w, h, draw in row:
            body.append(f'<g transform="translate({fmt(x)} {fmt(y)})">')
            body.extend(draw())
            body.append('</g>')
            x += w + gap
        y += max(p[1] for p in row) + gap
    return svg_document(int(round(width)), int(round(y - gap + margin)), title, description,
                        body)


# --------------------------------------------------------------------------
# Colorimetry

def rgb_to_xyz_matrix(primaries, white):
    xy = np.asarray(primaries, dtype=float)
    columns = np.vstack([xy[:, 0] / xy[:, 1], np.ones(3), (1 - xy[:, 0] - xy[:, 1]) / xy[:, 1]])
    wx, wy = white
    white_xyz = np.array([wx / wy, 1.0, (1 - wx - wy) / wy])
    scale = np.linalg.solve(columns, white_xyz)
    return columns * scale, scale


SRGB_XYZ, _ = rgb_to_xyz_matrix(SPACES['sRGB / BT.709']['primaries'], WHITES['D65'])
XYZ_SRGB = np.linalg.inv(SRGB_XYZ)


def lightness(y):
    y = np.asarray(y, dtype=float)
    return np.where(y > (6 / 29) ** 3, 116 * np.cbrt(y) - 16, y * (24389 / 27))


def cmf():
    data = np.array(CIE1931_CMF, dtype=float)
    return data[:, 0], data[:, 1:]


def spectrum_xy():
    wavelengths, xyz = cmf()
    total = xyz.sum(axis=1)
    keep = total > 0
    return wavelengths[keep], xyz[keep, 0] / total[keep], xyz[keep, 1] / total[keep]


def locus_polygon():
    """Spectral locus from 380 to 700 nm, closed by the line of purples."""
    wl, x, y = spectrum_xy()
    keep = wl <= 700
    return list(zip(x[keep], y[keep]))


def inside(points, polygon):
    px, py = points[..., 0], points[..., 1]
    result = np.zeros(px.shape, dtype=bool)
    n = len(polygon)
    for i in range(n):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % n]
        crosses = ((y1 > py) != (y2 > py)) & (
            px < (x2 - x1) * (py - y1) / (y2 - y1 + 1e-12) + x1)
        result ^= crosses
    return result


def chromaticity_colors(xs, ys):
    """Displayable sRGB colors for chromaticities, at maximum brightness."""
    xyz = np.stack([xs / ys, np.ones_like(xs), (1 - xs - ys) / ys], axis=-1)
    rgb = xyz @ XYZ_SRGB.T
    rgb = rgb - np.minimum(rgb.min(axis=-1, keepdims=True), 0)
    rgb = rgb / rgb.max(axis=-1, keepdims=True)
    return srgb_encode(rgb)


def planck_xy(temperature):
    wavelengths, xyz = cmf()
    lam = wavelengths * 1e-9
    radiance = 1 / (lam ** 5 * (np.exp(1.4388e-2 / (lam * temperature)) - 1))
    total = (xyz * radiance[:, None]).sum(axis=0)
    return total[0] / total.sum(), total[1] / total.sum()


def daylight_xy(temperature):
    t = temperature
    if t <= 7000:
        x = -4.6070e9 / t ** 3 + 2.9678e6 / t ** 2 + 0.09911e3 / t + 0.244063
    else:
        x = -2.0064e9 / t ** 3 + 1.9018e6 / t ** 2 + 0.24748e3 / t + 0.237040
    return x, -3.000 * x * x + 2.870 * x - 0.275


def xy_to_srgb_white(xy):
    """The encoded sRGB color of a white chromaticity, brightest channel at one."""
    x, y = xy
    rgb = XYZ_SRGB @ np.array([x / y, 1.0, (1 - x - y) / y])
    rgb = np.clip(rgb / rgb.max(), 0, 1)
    return srgb_encode(rgb)


# --------------------------------------------------------------------------
# Transfer functions

CURVES = {
    'sRGB': lambda v: np.where(v <= 0.04045, v / 12.92, ((v + 0.055) / 1.055) ** 2.4),
    '2.2': lambda v: v ** 2.2,
    '2.4 (BT.1886)': lambda v: v ** 2.4,
    '1.8': lambda v: v ** 1.8,
    '2.6 (DCI)': lambda v: v ** 2.6,
}
CURVE_STYLE = {
    'sRGB': (INK, ''), '2.2': (ACCENT, '6 3'), '2.4 (BT.1886)': (WARM, ''),
    '1.8': (GREEN, '2 3'), '2.6 (DCI)': (PURPLE, '6 3'),
}


def tf_ramps():
    patches, size = 11, 52
    width, height = patches * size + 40, 260

    def draw():
        body = []
        rows = (('Equal steps in stored sRGB values', np.linspace(0, 1, patches)),
                ('Equal steps in light (linear values, encoded for display)',
                 srgb_encode(np.linspace(0, 1, patches))))
        for r, (label, values) in enumerate(rows):
            top = 30 + r * 115
            body.append(svg_text(20, top - 10, label, 14, INK, 700))
            for i, v in enumerate(values):
                body.append(rect(20 + i * size, top, size, 56, hexrgb([v, v, v]), GRID))
                body.append(svg_text(20 + i * size + size / 2, top + 74, str(int(round(v * 255))),
                                     11, MUTED, 400, 'middle'))
        return body
    return compose([(width, height, draw)], 1, 'Two gray ramps',
                   'Two rows of eleven gray patches from black to white. The upper row steps '
                   'evenly in stored sRGB values and looks evenly spaced. The lower row steps '
                   'evenly in light; most of its patches look light and the jump from black to '
                   'the second patch is large. Numbers under the patches are the stored 8-bit '
                   'values.')


def tf_lightness():
    width, height = 620, 430

    def draw():
        ax = Axes(70, 20, 520, 330, (0, 1), (0, 1))
        body = ax.frame([0, 0.2, 0.4, 0.6, 0.8, 1], [0, 0.2, 0.4, 0.6, 0.8, 1],
                        'Relative luminance (light)', 'Value on a 0–1 scale', fmt, fmt)
        y = np.linspace(0, 1, 400)
        body.append(ax.curve(y, lightness(y) / 100, WARM, 3))
        body.append(ax.curve(y, srgb_encode(y), INK, 2.5, '6 3'))
        body.append(ax.curve(y, y, GRAY, 2, '2 3'))
        body += legend(330, 270, [('CIE lightness L* / 100', WARM, ''),
                                  ('sRGB encoded value', INK, '6 3'),
                                  ('Linear value', GRAY, '2 3')])
        return body
    return compose([(width, height, draw)], 1, 'Lightness and the sRGB encoding',
                   'CIE lightness divided by 100 and the sRGB encoded value plotted against '
                   'relative luminance. Both rise steeply near black and level off toward '
                   'white, and they nearly coincide; a linear value is a straight diagonal.')


def step_sizes(decode, bits=8):
    codes = np.arange(1 << bits) / ((1 << bits) - 1)
    lum = decode(codes)
    return np.arange(1, 1 << bits), np.diff(lightness(lum))


def tf_step_size():
    width, height = 620, 440

    def draw():
        ax = Axes(70, 20, 520, 330, (1, 255), (0.01, 20), ylog=True)
        body = ax.frame([32, 64, 128, 192, 255], [0.01, 0.1, 1, 10], '8-bit code value',
                        'Lightness step ΔL* to the next code', fmt,
                        lambda v: f'{v:g}')
        body.append(rect(ax.x, ax.py(1.0), ax.w, ax.py(0.5) - ax.py(1.0), '#fdf1e7'))
        body.append(svg_text(ax.x + ax.w - 6, ax.py(1.0) - 6,
                             'about the smallest visible step (ΔL* ≈ 0.5–1)', 12, MUTED, 400, 'end'))
        series = [('Linear', lambda v: v, GRAY, '2 3'), ('Power 1.8', CURVES['1.8'], GREEN, '2 3'),
                  ('sRGB', CURVES['sRGB'], INK, ''), ('Power 2.4', CURVES['2.4 (BT.1886)'], WARM, '')]
        for label, decode, color, dash in series:
            c, d = step_sizes(decode)
            keep = d >= ax.yrange[0]
            body.append(ax.curve(c[keep], d[keep], color, 2.5, dash))
        body += legend(400, 255, [(s[0], s[2], s[3]) for s in series])
        return body
    return compose([(width, height, draw)], 1, 'Lightness step of one code value',
                   'Lightness difference between neighboring 8-bit code values for linear '
                   'coding, a 1.8 power, sRGB and a 2.4 power, on a logarithmic axis. Linear '
                   'coding wastes steps far below visibility near white and jumps by several '
                   'lightness units near black; the gamma encodings stay close to or below '
                   'the visible step everywhere.')


def tf_dark_banding():
    width, height = 680, 300

    def draw():
        body = []
        ramp = np.linspace(0, 0.05, 640)[None, :].repeat(70, axis=0)
        linear8 = np.round(ramp * 255) / 255
        srgb8 = np.round(srgb_encode(ramp) * 255) / 255
        rows = (('8-bit linear values: 13 levels', srgb_encode(linear8)),
                ('8-bit sRGB values: 64 levels', srgb8))
        for r, (label, enc) in enumerate(rows):
            top = 30 + r * 120
            body.append(svg_text(20, top - 10, label, 14, INK, 700))
            rgb = np.repeat(enc[..., None], 3, axis=2)
            body.append(image(20, top, 640, 70, rgb))
        body.append(svg_text(20, 275, 'Both ramps run from black to 5% of white luminance.', 13,
                             MUTED))
        return body
    return compose([(width, height, draw)], 1, 'Dark gradient in 8 bits',
                   'A ramp from black to five percent of white luminance stored in 8 bits. '
                   'Stored as linear values it has 13 levels and shows wide bands; stored as '
                   'sRGB values it has 64 levels and looks smooth.')


def tf_curves(mobile):
    def full():
        ax = Axes(70, 20, 380, 300, (0, 1), (0, 1))
        body = ax.frame([0, 0.25, 0.5, 0.75, 1], [0, 0.25, 0.5, 0.75, 1], 'Encoded value',
                        'Relative luminance', fmt, fmt)
        v = np.linspace(0, 1, 300)
        for name, decode in CURVES.items():
            color, dash = CURVE_STYLE[name]
            body.append(ax.curve(v, decode(v), color, 2.5, dash))
        body += legend(90, 40, [(n, *CURVE_STYLE[n]) for n in CURVES], 12, 19)
        return body

    def near_black():
        ax = Axes(70, 20, 380, 300, (0.01, 0.3), (1e-5, 0.1), xlog=True, ylog=True)
        body = ax.frame([0.01, 0.03, 0.1, 0.3], [1e-5, 1e-4, 1e-3, 1e-2, 1e-1], 'Encoded value',
                        'Relative luminance', lambda v: f'{v:g}',
                        lambda v: f'{v:g}')
        v = np.logspace(-2, math.log10(0.3), 300)
        for name, decode in CURVES.items():
            color, dash = CURVE_STYLE[name]
            body.append(ax.curve(v, np.maximum(decode(v), 1e-5), color, 2.5, dash))
        body.append(svg_text(ax.x + 8, ax.y + 18, 'Near black, logarithmic axes', 12, MUTED))
        return body
    panels = [(470, 370, full), (470, 370, near_black)]
    return compose(panels, 1 if mobile else 2, 'Decoding curves',
                   'Relative luminance against encoded value for the sRGB curve and for '
                   'power curves of 1.8, 2.2, 2.4 and 2.6, over the full range and near black on '
                   'logarithmic axes. The curves differ most in the dark tones; near black the '
                   'sRGB curve becomes a straight line and gives far more light than a 2.2 '
                   'power.')


def tf_chain(mobile):
    steps = [('Scene light', 'linear'), ('Camera OETF', 'encode'), ('Stored values', 'encoded'),
             ('Display EOTF', 'decode'), ('Screen light', 'linear')]
    colors = {'linear': '#eef2f6', 'encode': '#fdf1e7', 'encoded': '#ffffff', 'decode': '#fdf1e7'}
    notes = {'Camera OETF': 'e.g. BT.709', 'Display EOTF': 'e.g. BT.1886, sRGB',
             'Stored values': 'pixels, files', 'Scene light': 'what the camera saw',
             'Screen light': 'what the viewer sees'}
    if mobile:
        width, height = 340, 560

        def draw():
            body = []
            for i, (name, kind) in enumerate(steps):
                y = 20 + i * 100
                body.append(rect(40, y, 220, 58, colors[kind], INK))
                body.append(svg_text(150, y + 25, name, 15, INK, 700, 'middle'))
                body.append(svg_text(150, y + 45, notes[name], 12, MUTED, 400, 'middle'))
                if i < len(steps) - 1:
                    body.append(line(150, y + 58, 150, y + 98, INK, 1.5))
            body.append(line(290, 49, 290, 449, WARM, 2, '5 4'))
            body.append(svg_text(305, 250, 'OOTF', 13, WARM, 700, 'middle').replace(
                '<text ', '<text transform="rotate(90 305 250)" ', 1))
            return body
    else:
        width, height = 980, 210

        def draw():
            body = []
            for i, (name, kind) in enumerate(steps):
                x = 10 + i * 195
                body.append(rect(x, 80, 160, 64, colors[kind], INK))
                body.append(svg_text(x + 80, 108, name, 15, INK, 700, 'middle'))
                body.append(svg_text(x + 80, 128, notes[name], 12, MUTED, 400, 'middle'))
                if i < len(steps) - 1:
                    body.append(line(x + 160, 112, x + 195, 112, INK, 1.5))
            body.append(f'<path d="M 90 76 C 300 30, 660 30, 870 76" stroke="{WARM}" '
                        'stroke-width="2" stroke-dasharray="5 4" fill="none"/>')
            body.append(svg_text(480, 26, 'OOTF: scene light to screen light', 13, WARM, 700,
                                 'middle'))
            body.append(svg_text(480, 180, 'The product of the two curves is the end-to-end '
                                 'response, deliberately not exactly 1.', 13, MUTED, 400,
                                 'middle'))
            return body
    return compose([(width, height, draw)], 1, 'OETF, EOTF and OOTF',
                   'Scene light passes through a camera opto-electronic transfer function into '
                   'stored values, and a display electro-optical transfer function turns them '
                   'back into light. The end-to-end opto-optical transfer function connects '
                   'scene light to screen light.')


def tf_mixing(mobile):
    def swatches():
        body = [svg_text(0, 16, 'Averaging red and green', 15, INK, 700)]
        items = [('Red', [1, 0, 0]), ('Green', [0, 1, 0]),
                 ('Average of stored values', [0.5, 0.5, 0]),
                 ('Average of light', srgb_encode(np.array([0.5, 0.5, 0.0])))]
        for i, (label, color) in enumerate(items):
            x = (i % 2) * 210
            y = 32 + (i // 2) * 120
            body.append(rect(x, y, 190, 70, hexrgb(color), GRID))
            code = np.round(np.clip(color, 0, 1) * 255).astype(int)
            body.append(svg_text(x, y + 88, label, 13, INK, 700))
            body.append(svg_text(x, y + 105, f'({code[0]}, {code[1]}, {code[2]})', 12, MUTED))
        return body

    def resize():
        body = [svg_text(0, 16, 'Shrinking fine detail by 4', 15, INK, 700)]
        size = 32
        yy, xx = np.mgrid[0:size * 4, 0:size * 4]
        lin = np.zeros((size * 4, size * 4, 3))
        top = yy < size * 2
        lin[top & ((xx % 4 == 0) | (yy % 4 == 0))] = 1.0
        stripe = (xx % 2 == 0)
        lin[~top & stripe] = [1, 0, 0]
        lin[~top & ~stripe] = [0, 1, 0]
        enc = srgb_encode(lin)
        crop = enc[size * 2 - 12:size * 2 + 12, :24]
        small_gamma = enc.reshape(size, 4, size, 4, 3).mean(axis=(1, 3))
        small_linear = srgb_encode(lin.reshape(size, 4, size, 4, 3).mean(axis=(1, 3)))
        items = (('Original, enlarged', crop), ('Stored values, averaged', small_gamma),
                 ('Light, averaged', small_linear))
        for i, (label, pic) in enumerate(items):
            x = i * 140
            body.append(image(x, 32, 128, 128, pic))
            body.append(rect(x, 32, 128, 128, 'none', GRID))
            for j, part in enumerate(label.split(', ')):
                body.append(svg_text(x, 180 + j * 17, part, 12, INK, 700 if j == 0 else 400))
        body.append(svg_text(0, 236, 'Top: thin white lines on black. Bottom: red and green '
                             'stripes.', 12, MUTED))
        return body
    panels = [(400, 260, swatches), (410, 260, resize)]
    return compose(panels, 1 if mobile else 2, 'Averaging stored values and light',
                   'Left: red and green averaged as stored values give a dark olive, '
                   '(128, 128, 0), while averaged as light they give the yellow that a fine red '
                   'and green pattern shows from a distance, (188, 188, 0). Right: a grid of '
                   'thin white lines and a red and green stripe pattern shrunk by four; '
                   'averaging stored values makes the lines and stripes darker than averaging '
                   'light.')


# --------------------------------------------------------------------------
# Primaries and white points

def convert_numbers(encoded, primaries, white):
    """Read encoded values with other primaries and show them in sRGB."""
    to_xyz, _ = rgb_to_xyz_matrix(primaries, white)
    lin = srgb_decode(encoded) @ (XYZ_SRGB @ to_xyz).T
    return srgb_encode(np.clip(lin, 0, 1)), float(((lin < 0) | (lin > 1)).any(axis=-1).mean())


def pw_misread(mobile):
    source = Image.open(ROOT / 'images/snake.png').convert('RGB').resize((300, 225),
                                                                       Image.Resampling.LANCZOS)
    enc = np.asarray(source, dtype=float) / 255
    cases = [('Read as sRGB (intended)', enc, 0.0)]
    for name in ('Display P3', 'BT.2020'):
        shown, clipped = convert_numbers(enc, SPACES[name]['primaries'], WHITES['D65'])
        cases.append((f'Same numbers read as {name}', shown, clipped))
    panels = []
    for label, pic, clipped in cases:
        def draw(label=label, pic=pic, clipped=clipped):
            body = [image(0, 0, 300, 225, pic, smooth=True), rect(0, 0, 300, 225, 'none', GRID),
                    svg_text(0, 248, label, 14, INK, 700)]
            if clipped:
                body.append(svg_text(0, 268, f'{clipped * 100:.0f}% of pixels clipped to sRGB',
                                     12, MUTED))
            return body
        panels.append((300, 275, draw))
    return compose(panels, 1 if mobile else 3, 'The same numbers with other primaries',
                   'A photograph of a snake shown three times from the same stored values: as '
                   'sRGB, as intended; read as Display P3; and read as BT.2020, each converted '
                   'to sRGB for display. Wider primaries make every color more saturated, and '
                   'some colors fall outside sRGB and are clipped.')


def chroma_axes(size, x0=60, y0=20):
    return Axes(x0, y0, size, size * 0.9 / 0.8, (0, 0.8), (0, 0.9))


def draw_locus(ax, filled=True, ticks=True):
    body = []
    polygon = locus_polygon()
    if filled:
        n = 360
        xs = np.linspace(0, 0.8, n)
        ys = np.linspace(0.9, 0, int(n * 0.9 / 0.8))
        gx, gy = np.meshgrid(xs, np.maximum(ys, 1e-4))
        mask = inside(np.stack([gx, gy], axis=-1), polygon)
        colors = chromaticity_colors(gx, gy)
        colors[~mask] = 1.0
        body.append(image(ax.px(0), ax.py(0.9), ax.w, ax.h, colors, smooth=True))
    pts = [(ax.px(x), ax.py(y)) for x, y in polygon]
    body.append(polyline(pts, INK, 1.5, close=True))
    if ticks:
        wl, x, y = spectrum_xy()
        for w in (460, 480, 490, 500, 510, 520, 540, 560, 580, 600, 620, 700):
            i = int(np.where(wl == w)[0][0])
            dx, dy = x[i] - 0.33, y[i] - 0.33
            norm = math.hypot(dx, dy)
            tx = ax.px(x[i]) + dx / norm * 16
            ty = ax.py(y[i]) - dy / norm * 16
            body.append(f'<circle cx="{ax.px(x[i]):.1f}" cy="{ax.py(y[i]):.1f}" r="2" '
                        f'fill="{INK}"/>')
            body.append(svg_text(tx, ty + 4, str(w), 11, MUTED, 400, 'middle'))
    return body


def pw_chromaticity():
    width, height = 560, 600

    def draw():
        ax = chroma_axes(460)
        body = ax.frame([0, 0.2, 0.4, 0.6, 0.8], [0, 0.2, 0.4, 0.6, 0.8], 'x', 'y', fmt, fmt)
        body += draw_locus(ax)
        body.append(svg_text(ax.px(0.53), ax.py(0.09), 'line of purples', 12, MUTED, 400,
                             'middle'))
        body.append(svg_text(ax.px(0.42), ax.py(0.86), 'wavelengths in nm', 12, MUTED))
        return body
    return compose([(width, height, draw)], 1, 'The CIE 1931 chromaticity diagram',
                   'The horseshoe of the CIE 1931 xy chromaticity diagram, colored with an '
                   'approximation of each chromaticity at full brightness, with wavelengths '
                   'along the spectral locus and the straight line of purples closing it. '
                   'Colors outside sRGB are shown at the nearest displayable color.')


def pw_gamuts(mobile):
    size = 440

    def plot():
        ax = chroma_axes(size)
        body = ax.frame([0, 0.2, 0.4, 0.6, 0.8], [0, 0.2, 0.4, 0.6, 0.8], 'x', 'y', fmt, fmt)
        body += draw_locus(ax, filled=False, ticks=False)
        for name, spec in SPACES.items():
            pts = [(ax.px(x), ax.py(y)) for x, y in spec['primaries']]
            body.append(polyline(pts, spec['color'], 2.2, spec['dash'], close=True))
        for name in ('D65', 'D50', 'C'):
            x, y = WHITES[name]
            body.append(f'<circle cx="{ax.px(x):.1f}" cy="{ax.py(y):.1f}" r="3" fill="{INK}"/>')
        body.append(svg_text(ax.px(0.3127) - 8, ax.py(0.3290) + 4, 'D65', 11, INK, 400, 'end'))
        body.append(svg_text(ax.px(0.3457) + 8, ax.py(0.3585) + 4, 'D50', 11, INK))
        body.append(svg_text(ax.px(0.3101) - 8, ax.py(0.3162) + 12, 'C', 11, INK, 400, 'end'))
        return body

    def key():
        entries = [(n, s['color'], s['dash']) for n, s in SPACES.items()]
        return legend(10, 20, entries, 13, 24)
    panels = [(size + 70, size * 0.9 / 0.8 + 70, plot), (220, 190, key)]
    return compose(panels, 1 if mobile else 2, 'Gamuts of common RGB spaces',
                   'Triangles of the primaries of NTSC 1953, SMPTE-C, sRGB and BT.709, Adobe '
                   'RGB (1998), Display P3, BT.2020 and ProPhoto RGB inside the spectral locus. '
                   'The ProPhoto blue and green primaries lie outside the locus.')


def pw_whites(mobile):
    marks = [('D65', WHITES['D65']), ('D50', WHITES['D50']), ('C', WHITES['C']),
             ('DCI', WHITES['DCI']), ('E', WHITES['E'])]

    def plot():
        ax = Axes(60, 20, 420, 330, (0.26, 0.46), (0.26, 0.42))
        body = ax.frame([0.26, 0.30, 0.34, 0.38, 0.42, 0.46], [0.26, 0.30, 0.34, 0.38, 0.42],
                        'x', 'y', lambda v: f'{v:.2f}', lambda v: f'{v:.2f}')
        temps = np.linspace(2900, 20000, 200)
        planck = [p for p in (planck_xy(t) for t in temps)
                  if ax.xrange[0] <= p[0] <= ax.xrange[1] and ax.yrange[0] <= p[1] <= ax.yrange[1]]
        body.append(ax.curve([p[0] for p in planck], [p[1] for p in planck], GRAY, 2.5))
        for t in (3000, 4000, 5000, 6500, 10000):
            x, y = planck_xy(t)
            if ax.xrange[0] < x < ax.xrange[1]:
                body.append(f'<circle cx="{ax.px(x):.1f}" cy="{ax.py(y):.1f}" r="2.5" '
                            f'fill="{GRAY}"/>')
                if t in (3000, 4000, 10000):
                    body.append(svg_text(ax.px(x) - 6, ax.py(y) + 16, f'{t} K', 11, MUTED,
                                         400, 'end'))
        day = [p for p in (daylight_xy(t) for t in np.linspace(4000, 20000, 120))
               if ax.xrange[0] <= p[0] <= ax.xrange[1] and ax.yrange[0] <= p[1] <= ax.yrange[1]]
        body.append(ax.curve([p[0] for p in day], [p[1] for p in day], ORANGE, 2, '5 3'))
        for name, (x, y) in marks:
            body.append(f'<circle cx="{ax.px(x):.1f}" cy="{ax.py(y):.1f}" r="4.5" fill="{INK}"/>')
            body.append(svg_text(ax.px(x) + 8, ax.py(y) + 15, name, 12, INK, 700))
        body += legend(260, 290, [('Black-body (Planckian) locus', GRAY, ''),
                                  ('CIE daylight locus', ORANGE, '5 3')], 12, 19)
        return body

    def swatches():
        body = [svg_text(0, 16, 'Each white on a D65 sRGB display', 14, INK, 700)]
        for i, (name, xy) in enumerate(marks):
            y = 34 + i * 62
            body.append(rect(0, y, 120, 48, hexrgb(xy_to_srgb_white(xy)), GRID))
            body.append(svg_text(132, y + 29, name, 14, INK, 700))
        return body
    panels = [(500, 400, plot), (270, 360, swatches)]
    return compose(panels, 1 if mobile else 2, 'White points',
                   'The black-body locus with color temperatures, the CIE daylight locus, and '
                   'the white points D65, D50, C, DCI and E, together with swatches showing '
                   'how each white looks when shown without adaptation on a display whose '
                   'white is D65: D50 and DCI look warm, E looks pinkish.')


def pw_barycenter():
    width, height = 560, 560
    primaries = SPACES['sRGB / BT.709']['primaries']
    _, scale = rgb_to_xyz_matrix(primaries, WHITES['D65'])
    xy = np.asarray(primaries)
    weights = scale / xy[:, 1]

    def draw():
        ax = Axes(60, 20, 460, 460 * 0.65 / 0.6, (0.1, 0.7), (0.0, 0.65))
        body = ax.frame([0.1, 0.3, 0.5, 0.7], [0, 0.2, 0.4, 0.6], 'x', 'y', fmt, fmt)
        pts = [(ax.px(x), ax.py(y)) for x, y in primaries]
        body.append(polyline(pts, INK, 1.5, close=True))
        wx, wy = WHITES['D65']
        colors = ('#d33', '#2a2', '#33d')
        for (x, y), w, color, name in zip(primaries, weights, colors, ('R', 'G', 'B')):
            r = 10 + 30 * math.sqrt(w / weights.max())
            body.append(f'<circle cx="{ax.px(x):.1f}" cy="{ax.py(y):.1f}" r="{r:.1f}" '
                        f'fill="{color}" fill-opacity="0.35" stroke="{color}"/>')
            body.append(line(ax.px(x), ax.py(y), ax.px(wx), ax.py(wy), color, 1, '4 3'))
            body.append(svg_text(ax.px(x), ax.py(y) - r - 6, f'{name}: weight {w / weights.sum():.2f}',
                                 12, INK, 700, 'middle'))
        body.append(f'<circle cx="{ax.px(wx):.1f}" cy="{ax.py(wy):.1f}" r="5" fill="{INK}"/>')
        body.append(svg_text(ax.px(wx) + 10, ax.py(wy) + 4, 'D65 white', 12, INK, 700))
        return body
    return compose([(width, height, draw)], 1, 'White as the balance point of the primaries',
                   'The sRGB primaries in the xy diagram with circles sized by the share of '
                   'X + Y + Z each contributes to white. D65 is their weighted center: blue '
                   'contributes the largest share of X + Y + Z although it adds the least '
                   'luminance.')


# --------------------------------------------------------------------------
# Numbers quoted in the guides

def manifest_numbers():
    numbers = {}
    m, _ = rgb_to_xyz_matrix(SPACES['sRGB / BT.709']['primaries'], WHITES['D65'])
    numbers['srgb_to_xyz_d65'] = np.round(m, 4).tolist()
    smptec, _ = rgb_to_xyz_matrix(SPACES['SMPTE-C']['primaries'], WHITES['D65'])
    to_smptec = np.linalg.inv(smptec) @ m
    numbers['linear_srgb_to_smpte_c'] = np.round(to_smptec, 5).tolist()
    p3, _ = rgb_to_xyz_matrix(SPACES['Display P3']['primaries'], WHITES['D65'])
    numbers['display_p3_red_in_linear_srgb'] = np.round(XYZ_SRGB @ p3 @ [1, 0, 0], 5).tolist()
    _, scale = rgb_to_xyz_matrix(SPACES['sRGB / BT.709']['primaries'], WHITES['D65'])
    xy = np.asarray(SPACES['sRGB / BT.709']['primaries'])
    w = scale / xy[:, 1]
    numbers['srgb_white_weights_xyz_sum'] = np.round(w / w.sum(), 3).tolist()
    numbers['srgb_white_luminance_shares'] = np.round(scale, 4).tolist()
    for name, decode in (('linear', lambda v: v), ('srgb', CURVES['sRGB'])):
        c, d = step_sizes(decode)
        numbers[f'delta_l_{name}'] = {'first_step': round(float(d[0]), 2),
                                      'max': round(float(d.max()), 2),
                                      'last_step': round(float(d[-1]), 3)}
        lum = decode(np.arange(256) / 255)
        numbers[f'levels_below_5_percent_{name}'] = int((lum <= 0.05).sum())
    numbers['mid_gray'] = {'stored_average': 128,
                           'light_average': int(round(float(srgb_encode(0.5)) * 255))}
    numbers['srgb_vs_2_2_at_code'] = {str(c): {'srgb': round(float(CURVES['sRGB'](c / 255)), 5),
                                               '2.2': round(float((c / 255) ** 2.2), 5)}
                                      for c in (5, 10, 20, 64, 128)}
    numbers['daylight_6504k_xy'] = [round(v, 4) for v in daylight_xy(6504)]
    numbers['planck_6500k_xy'] = [round(float(v), 4) for v in planck_xy(6500)]
    return numbers


FIGURES = {
    'tf-ramps.svg': tf_ramps,
    'tf-lightness.svg': tf_lightness,
    'tf-step-size.svg': tf_step_size,
    'tf-dark-banding.svg': tf_dark_banding,
    'tf-curves-wide.svg': lambda: tf_curves(False),
    'tf-curves-mobile.svg': lambda: tf_curves(True),
    'tf-chain-wide.svg': lambda: tf_chain(False),
    'tf-chain-mobile.svg': lambda: tf_chain(True),
    'tf-mixing-wide.svg': lambda: tf_mixing(False),
    'tf-mixing-mobile.svg': lambda: tf_mixing(True),
    'pw-misread-wide.svg': lambda: pw_misread(False),
    'pw-misread-mobile.svg': lambda: pw_misread(True),
    'pw-chromaticity.svg': pw_chromaticity,
    'pw-gamuts-wide.svg': lambda: pw_gamuts(False),
    'pw-gamuts-mobile.svg': lambda: pw_gamuts(True),
    'pw-whites-wide.svg': lambda: pw_whites(False),
    'pw-whites-mobile.svg': lambda: pw_whites(True),
    'pw-barycenter.svg': pw_barycenter,
}
MANIFEST = 'foundations.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {name: draw() for name, draw in FIGURES.items()}
    assets[MANIFEST] = json.dumps({
        'generator': 'tools/plot_colorspace_foundation_figures.py',
        'plot_dependency': 'numpy, Pillow',
        'generated': sorted(FIGURES),
        'sources': {
            'cmf': 'CIE 1931 2-degree standard observer, 5 nm, CIE 015:2018 as tabulated by CVRL',
            'primaries': 'CSS Color 4 (sRGB, Display P3, a98-rgb, prophoto-rgb, rec2020), '
                         'SMPTE RP 145 (SMPTE-C), FCC 1953 (NTSC)',
            'whites': 'CSS Color 4 (D50, D65); CIE Illuminant C; SMPTE RP 431-2 (DCI)',
            'daylight_locus': 'CIE 015 daylight chromaticity formula',
            'photograph': 'images/snake.png',
        },
        'spaces': {n: {'primaries': s['primaries'], 'white': s['white']}
                   for n, s in SPACES.items()},
        'whites': WHITES,
        'numbers': manifest_numbers(),
    }, indent=2, ensure_ascii=False) + '\n'
    if not args.check:
        OUT.mkdir(parents=True, exist_ok=True)
    for name, content in assets.items():
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit(f'Stale figure: {path}')
        else:
            path.write_text(content)
    print(f'{"Verified" if args.check else "Generated"} {len(assets)} assets')


if __name__ == '__main__':
    main()
