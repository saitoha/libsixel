#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the figures and measurements of the character cell perception document.

Use NumPy and Pillow; the sensitivity models, color conversions and SVG
helpers are shared with tools/plot_visual_perception_figures.py, which
also requires Matplotlib to be importable.  Run without arguments to
regenerate docs/quality/character-cell-perception/figures, or with
--check to verify that the committed files are current.  The figures are
hand-written SVG with embedded PNG images, so their bytes do not depend on
the Matplotlib version.
"""

from __future__ import annotations

import argparse
import base64
import functools
import io
import json
import math

import numpy as np
from PIL import Image

from plot_visual_perception_figures import (CSF_GRID, GRID, INK, L_COLOR, MUTED, ROOT,
                                            S_COLOR, chroma_csf, luminance_csf,
                                            srgb_decode, srgb_encode, svg_document,
                                            svg_text)

OUT = ROOT / 'docs/quality/character-cell-perception/figures'


# --------------------------------------------------------------------------
# Character cells on the sensitivity curves

CSF_SAMPLE_CPD = (0.5, 1, 2, 3, 4, 8, 12, 17, 30)

# The character-cell example of the visual perception guide: a 100 ppi
# display viewed at 50 cm, with 8 x 16 pixel character cells.
CELL_EXAMPLE = {'ppi': 100, 'distance_mm': 500, 'cell_px': (8, 16)}


def csf_samples():
    """Return the tabulated luminance and chroma sensitivities."""
    rows = []
    for f in CSF_SAMPLE_CPD:
        lum = float(luminance_csf(f))
        chroma = float(chroma_csf(f))
        rows.append({'cpd': f, 'luminance': round(lum, 4),
                     'chroma': round(chroma, 4),
                     'luminance_over_chroma': round(lum / chroma, 2)})
    return rows


def character_cell_examples():
    """Return the finest pattern frequency of common character renderings."""
    pitch = 25.4 / CELL_EXAMPLE['ppi']
    ppd = CELL_EXAMPLE['distance_mm'] * math.tan(math.radians(1)) / pitch
    width, height = CELL_EXAMPLE['cell_px']
    units = (
        ('character cell', width, height),
        ('half block', width, height / 2),
        ('chafa 8 x 8 sub-cell', width / 8, height / 8),
    )
    rows = []
    for name, unit_w, unit_h in units:
        rows.append({'unit': name, 'unit_px': [float(unit_w), float(unit_h)],
                     'horizontal_cpd': round(ppd / (2 * unit_w), 2),
                     'vertical_cpd': round(ppd / (2 * unit_h), 2)})
    return {'display': CELL_EXAMPLE, 'pixels_per_degree': round(ppd, 2),
            'units': rows}


def csf_ratio():
    """Ratio of luminance to chroma sensitivity with the character-cell bands."""
    width, height = 760, 335
    x0, x1 = 90, 700
    lo, hi = math.log10(0.3), math.log10(60)
    y0, y1 = 50, 270
    rlo, rhi = -1.0, 3.0

    def fx(f):
        return x0 + (math.log10(f) - lo) / (hi - lo) * (x1 - x0)

    def fy(r):
        return y1 - (math.log10(r) - rlo) / (rhi - rlo) * (y1 - y0)

    cells = character_cell_examples()['units']
    cell = next(u for u in cells if u['unit'] == 'character cell')
    sub = next(u for u in cells if u['unit'] == 'chafa 8 x 8 sub-cell')
    body = []
    for f_lo, f_hi, label in ((cell['vertical_cpd'], cell['horizontal_cpd'], 'Cell grid'),
                              (sub['vertical_cpd'], sub['horizontal_cpd'],
                               'Shapes inside a cell')):
        body.append(f'<rect x="{fx(f_lo):.1f}" y="{y0}" width="{fx(f_hi) - fx(f_lo):.1f}" '
                    f'height="{y1 - y0}" fill="#eef2f6"/>')
        body.append(svg_text((fx(f_lo) + fx(f_hi)) / 2, y0 - 10, label, 13, MUTED, 700,
                             'middle'))
    body.append(f'<rect x="{x0}" y="{y0}" width="{x1 - x0}" height="{y1 - y0}" '
                f'fill="none" stroke="{GRID}"/>')
    for f in (0.5, 1, 2, 5, 10, 20, 50):
        x = fx(f)
        body.append(f'<path d="M {x:.1f} {y0} L {x:.1f} {y1}" stroke="{GRID}"/>')
        body.append(svg_text(x, y1 + 20, f'{f:g}', 13, MUTED, 400, 'middle'))
    body.append(svg_text((x0 + x1) / 2, y1 + 44, 'Spatial frequency (cycles per degree)',
                         14, INK, 400, 'middle'))
    for r in (0.1, 1, 10, 100, 1000):
        y = fy(r)
        body.append(f'<path d="M {x0} {y:.1f} L {x1} {y:.1f}" stroke="{GRID}"/>')
        body.append(svg_text(x0 - 8, y + 4, f'{r:g}', 13, MUTED, 400, 'end'))
    body.append(f'<path d="M {x0} {fy(1):.1f} L {x1} {fy(1):.1f}" stroke="{MUTED}" '
                'stroke-dasharray="5 4"/>')
    body.append(svg_text(x1 - 4, fy(1) - 6, 'equal', 12, MUTED, 400, 'end'))
    body.append(svg_text(x0 + 6, fy(300), 'luminance dominates', 12, MUTED, 400, 'start'))
    body.append(svg_text(x1 - 6, fy(0.15), 'chroma dominates', 12, MUTED, 400, 'end'))
    middle = (y0 + y1) / 2
    body.append(svg_text(30, middle, 'Luminance ÷ chroma sensitivity', 14, INK, 400, 'middle')
                .replace('<text ', f'<text transform="rotate(-90 30 {middle})" ', 1))
    # The chroma curve is schematic at high frequencies; draw the ratio only
    # over the tabulated range.
    grid = np.logspace(lo, math.log10(max(CSF_SAMPLE_CPD)), 300)
    ratio = np.clip(luminance_csf(grid) / chroma_csf(grid), 10 ** rlo, 10 ** rhi)
    points = ' '.join(f'{fx(f):.1f},{fy(r):.1f}' for f, r in zip(grid, ratio))
    body.append(f'<polyline points="{points}" fill="none" stroke="{INK}" stroke-width="2.5"/>')
    for row in csf_samples():
        f, r = row['cpd'], row['luminance_over_chroma']
        x, y = fx(f), fy(r)
        body.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.5" fill="{INK}"/>')
        label = f'{r:.2f}' if r < 1 else f'{r:.1f}' if r < 10 else f'{r:.0f}'
        if r < 0.3:
            body.append(svg_text(x - 6, y - 8, label, 12, INK, 400, 'end'))
        elif r < 1:
            body.append(svg_text(x + 6, y + 16, label, 12, INK, 400, 'start'))
        else:
            body.append(svg_text(x - 6, y - 8, label, 12, INK, 400, 'end'))
    return svg_document(width, height, 'Luminance and chroma sensitivity compared',
                        'The ratio of the relative luminance sensitivity to the relative '
                        'chroma sensitivity of the visual perception chapter on logarithmic axes: 0.21 at 0.5 '
                        'cycles per degree, 0.45 at 1, 1.6 at 2, 41 at 8 and 137 at 17. A '
                        'shaded band from about 1 to 2 cycles per degree marks the cell grid '
                        'and another from about 9 to 17 the shapes inside a cell.',
                        body)


# --------------------------------------------------------------------------
# Fine pixels and coarse tiles
#
# The presentation was chosen with the interactive tool in
# tools/docs/tile-lab/index.html.  Averages and dithering errors are formed
# in linear light, nearest palette colors are chosen by DIN99d, and
# Floyd-Steinberg diffusion runs in raster order.  Visible error is
# estimated in the manner of S-CIELAB (Zhang and Wandell 1997): both images
# are converted to the opponent space of S-CIELAB, filtered with the
# luminance and chroma sensitivity curves of this chapter for a stated
# viewing geometry, and compared in CIELAB.

TILE_DEMO = {'image': 'images/snake.png', 'size': (360, 270), 'tile': 9,
             'figure_bits': 3, 'ppi': 192, 'distance_mm': 500}
HALF_BLOCK = {'columns': 120, 'rows': 45, 'precision_bits': 7}

# XYZ (CIE 1931 2 degree) to the S-CIELAB opponent space, from cmatrix.m of
# the S-CIELAB code distributed with ISETCam.
XYZ_TO_OPPONENT = np.array([[278.7336, 721.8031, -106.5520],
                            [-448.7736, 289.8056, 77.1569],
                            [85.9513, -589.9859, 501.1089]]) / 1000
SRGB_TO_XYZ = np.array([[0.4124564, 0.3575761, 0.1804375],
                        [0.2126729, 0.7151522, 0.0721750],
                        [0.0193339, 0.1191920, 0.9503041]])


def linear_to_lab(rgb, din99d=False):
    xyz = np.asarray(rgb, dtype=float) @ SRGB_TO_XYZ.T
    if din99d:
        xyz = xyz.copy()
        xyz[..., 0] = 1.12 * xyz[..., 0] - 0.12 * xyz[..., 2]
    t = xyz / np.array([0.95047, 1.0, 1.08883])
    f = np.where(t > 216 / 24389, np.cbrt(t), (24389 / 27 * t + 16) / 116)
    lab = np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]),
                    200 * (f[..., 1] - f[..., 2])], axis=-1)
    if not din99d:
        return lab
    c50, s50 = math.cos(math.radians(50)), math.sin(math.radians(50))
    e = c50 * lab[..., 1] + s50 * lab[..., 2]
    ff = 1.14 * (c50 * lab[..., 2] - s50 * lab[..., 1])
    chroma = 22.5 * np.log1p(0.06 * np.hypot(e, ff))
    hue = np.arctan2(ff, e) + math.radians(50)
    return np.stack([325.22 * np.log1p(0.0036 * lab[..., 0]),
                     chroma * np.cos(hue), chroma * np.sin(hue)], axis=-1)


def linear_to_oklab(rgb):
    rgb = np.asarray(rgb, dtype=float)
    lms = np.cbrt(rgb @ np.array([[0.4122214708, 0.5363325363, 0.0514459929],
                                  [0.2119034982, 0.6806995451, 0.1073969566],
                                  [0.0883024619, 0.2817188376, 0.6299787005]]).T)
    return lms @ np.array([[0.2104542553, 0.7936177850, -0.0040720468],
                           [1.9779984951, -2.4285922050, 0.4505937099],
                           [0.0259040371, 0.7827717662, -0.8086757660]]).T


def nearest_palette(colors, palette_din):
    din = linear_to_lab(np.clip(colors, 0, 1), din99d=True).reshape(-1, 3)
    out = np.empty(len(din), dtype=int)
    for start in range(0, len(din), 4096):
        chunk = din[start:start + 4096]
        out[start:start + 4096] = np.argmin(
            ((chunk[:, None, :] - palette_din[None, :, :]) ** 2).sum(axis=-1), axis=1)
    return out.reshape(np.shape(colors)[:-1])


class Palette:
    """Palette colors in linear light with a DIN99d nearest-color search.

    A uniform palette, whose entries are all combinations of evenly spaced
    encoded levels, searches only the 27 entries around the rounded color.
    """

    def __init__(self, colors, uniform_bits=None):
        self.colors = np.asarray(colors, dtype=float)
        self.din = linear_to_lab(self.colors, din99d=True)
        self.bits = uniform_bits

    def nearest(self, image):
        image = np.clip(np.asarray(image, dtype=float), 0, 1)
        if self.bits is None:
            return nearest_palette(image, self.din)
        scale = (1 << self.bits) - 1
        flat = image.reshape(-1, 3)
        base = np.round(srgb_encode(flat) * scale).astype(int)
        steps = np.stack(np.meshgrid(*[[-1, 0, 1]] * 3, indexing='ij'), -1).reshape(-1, 3)
        cand = np.clip(base[:, None, :] + steps[None], 0, scale)
        index = (cand[..., 0] * (scale + 1) + cand[..., 1]) * (scale + 1) + cand[..., 2]
        target = linear_to_lab(flat, din99d=True)
        best = np.argmin(((self.din[index] - target[:, None, :]) ** 2).sum(-1), axis=1)
        return index[np.arange(len(flat)), best].reshape(image.shape[:-1])


def diffuse_to_palette(image, palette):
    work = image.astype(float).copy()
    height, width, _ = work.shape
    out = np.zeros((height, width), dtype=int)
    memo = {}
    for y in range(height):
        for x in range(width):
            value = np.clip(work[y, x], 0, 1)
            key = tuple(np.round(value * 1023).astype(int))
            index = memo.get(key)
            if index is None:
                index = int(palette.nearest(value[None, :])[0])
                memo[key] = index
            out[y, x] = index
            error = value - palette.colors[index]
            if x + 1 < width:
                work[y, x + 1] += error * 7 / 16
            if y + 1 < height:
                if x > 0:
                    work[y + 1, x - 1] += error * 3 / 16
                work[y + 1, x] += error * 5 / 16
                if x + 1 < width:
                    work[y + 1, x + 1] += error * 1 / 16
    return palette.colors[out]


def map_to_palette(image, palette):
    return palette.colors[palette.nearest(image)]


def uniform_palette(bits):
    levels = np.arange(1 << bits) / ((1 << bits) - 1)
    grid = np.stack(np.meshgrid(levels, levels, levels, indexing='ij'), axis=-1)
    return Palette(srgb_decode(grid.reshape(-1, 3)), uniform_bits=bits)


def round_bits(image, bits):
    scale = (1 << bits) - 1
    return srgb_decode(np.round(srgb_encode(image) * scale) / scale)


def adaptive_palette(image, count, iterations=30):
    """Median cut refined by k-means in OKLab; entries rounded to 8 bits."""
    pixels = np.clip(image.reshape(-1, 3), 0, 1)
    codes = np.round(srgb_encode(pixels) * 255).astype(np.uint8)
    seed = Image.fromarray(codes.reshape(1, -1, 3)).quantize(
        colors=count, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    entries = len(seed.getcolors(count * 4))
    palette = srgb_decode(np.array(seed.getpalette()[:3 * entries], dtype=float)
                          .reshape(-1, 3) / 255)
    points = linear_to_oklab(pixels)
    for _ in range(iterations):
        centers = linear_to_oklab(palette)
        assign = np.argmin(((points[:, None, :] - centers[None]) ** 2).sum(-1), axis=1)
        for k in range(len(palette)):
            members = assign == k
            if members.any():
                palette[k] = pixels[members].mean(axis=0)
    return Palette(round_bits(palette, 8))


def image_units(path, columns, rows):
    source = Image.open(ROOT / path).convert('RGB')
    linear = srgb_decode(np.asarray(source, dtype=float) / 255)
    planes = [np.asarray(Image.fromarray(linear[..., k].astype(np.float32), 'F')
                         .resize((columns, rows), Image.Resampling.BOX)) for k in range(3)]
    return np.clip(np.stack(planes, axis=-1).astype(float), 0, 1)


def sunset_gradient(width, height):
    x = np.linspace(0, 1, width)[None, :]
    y = np.linspace(0, 1, height)[:, None]
    distance = np.hypot(x - 0.7, (y - 0.72) * 0.75)
    sun = np.exp(-(distance / 0.08) ** 2)
    glow = np.exp(-(distance / 0.35) ** 2) * 0.35
    encoded = np.stack([0.10 + 0.75 * y ** 1.5 + 0 * x + sun + glow,
                        0.22 + 0.45 * y + 0.05 * x + 0.8 * sun + 0.6 * glow,
                        0.55 + 0.25 * (1 - y) + 0 * x + 0.4 * sun + 0.2 * glow], axis=-1)
    return srgb_decode(np.clip(encoded, 0, 1))


def tile_means(image, tile):
    height, width, _ = image.shape
    return image.reshape(height // tile, tile, width // tile, tile, 3).mean(axis=(1, 3))


def expand(grid, factor):
    return np.repeat(np.repeat(grid, factor, axis=0), factor, axis=1)


def pixels_per_degree(ppi, distance_mm):
    return distance_mm * math.tan(math.radians(1)) / (25.4 / ppi)


def visible_error(reference, output, ppd):
    """Mean and 99th percentile CIELAB difference after spatial filtering."""
    height, width, _ = reference.shape
    fy = np.fft.fftfreq(height)[:, None] * ppd
    fx = np.fft.fftfreq(width)[None, :] * ppd
    radial = np.hypot(fx, fy)
    peak = CSF_GRID[np.argmax(luminance_csf(CSF_GRID))]
    filters = [np.where(radial < peak, 1.0, luminance_csf(np.maximum(radial, 1e-6))),
               chroma_csf(radial), chroma_csf(radial)]
    to_opponent = XYZ_TO_OPPONENT @ SRGB_TO_XYZ
    from_opponent = np.linalg.inv(to_opponent)

    def filtered(image):
        opponent = image @ to_opponent.T
        planes = [np.real(np.fft.ifft2(np.fft.fft2(opponent[..., k]) * filters[k]))
                  for k in range(3)]
        return np.clip(np.stack(planes, axis=-1) @ from_opponent.T, 0, None)

    delta = np.linalg.norm(linear_to_lab(filtered(reference)) - linear_to_lab(filtered(output)),
                           axis=-1)
    return round(float(delta.mean()), 2), round(float(np.percentile(delta, 99)), 2)


@functools.lru_cache(maxsize=1)
def tile_study():
    width, height = TILE_DEMO['size']
    tile = TILE_DEMO['tile']
    ppd = pixels_per_degree(TILE_DEMO['ppi'], TILE_DEMO['distance_mm'])
    snake = image_units(TILE_DEMO['image'], width, height)
    x = np.linspace(0, 1, width)[None, :, None]
    y = np.linspace(0, 1, height)[:, None, None]
    gradient = srgb_decode(np.clip(np.concatenate(
        [0.85 - 0.6 * x + 0 * y, 0.55 + 0.25 * x - 0.2 * y, 0.35 + 0.5 * x - 0.25 * y], -1), 0, 1))
    dark = srgb_decode(np.clip(np.concatenate(
        [0.02 + 0.18 * x + 0 * y, 0.03 + 0.12 * x + 0 * y, 0.06 + 0.20 * x + 0 * y], -1), 0, 1))
    images = {'snake': snake, 'gradient': gradient, 'dark gradient': dark}

    gains = []
    for name in ('snake', 'gradient'):
        image = images[name]
        tiles = expand(tile_means(image, tile), tile)
        means = tile_means(image, tile)
        palettes = [(f'uniform {b} bits per channel', uniform_palette(b)) for b in (3, 4, 5)]
        palettes += [(f'adaptive {n}', adaptive_palette(image, n)) for n in (64, 256)]
        for label, palette in palettes:
            row = {'image': name, 'palette': label}
            row['pixel_none'] = visible_error(image, map_to_palette(image, palette), ppd)[0]
            row['pixel_dithered'] = visible_error(image, diffuse_to_palette(image, palette), ppd)[0]
            row['tile_none'] = visible_error(tiles, expand(map_to_palette(means, palette), tile), ppd)[0]
            row['tile_dithered'] = visible_error(
                tiles, expand(diffuse_to_palette(means, palette), tile), ppd)[0]
            row['pixel_gain'] = round(row['pixel_none'] / row['pixel_dithered'], 2)
            row['tile_gain'] = round(row['tile_none'] / row['tile_dithered'], 2)
            gains.append(row)

    precision = []
    for name, image in images.items():
        means = tile_means(image, tile)
        tiles = expand(means, tile)
        cases = [(f'{b} bits per channel', round_bits(means, b)) for b in (5, 6, 7, 8)]
        cases.insert(1, ('5-6-5 bits', srgb_decode(np.stack(
            [np.round(srgb_encode(means[..., k]) * ((1 << b) - 1)) / ((1 << b) - 1)
             for k, b in enumerate((5, 6, 5))], axis=-1))))
        cases.append(('adaptive 256', map_to_palette(means, adaptive_palette(means, 256))))
        for label, out in cases:
            mean, p99 = visible_error(tiles, expand(out, tile), ppd)
            precision.append({'image': name, 'colors': label, 'mean': mean, 'p99': p99})

    bits = TILE_DEMO['figure_bits']
    palette = uniform_palette(bits)
    means = tile_means(snake, tile)
    panels = {
        'fine': [('Original', snake), ('Dithered', diffuse_to_palette(snake, palette)),
                 ('Not dithered', map_to_palette(snake, palette))],
        'tile': [('24-bit mean of each tile', expand(means, tile)),
                 ('Dithered over tiles', expand(diffuse_to_palette(means, palette), tile)),
                 ('Not dithered', expand(map_to_palette(means, palette), tile))],
    }
    return {'ppd': round(ppd, 1), 'gains': gains, 'precision': precision, 'panels': panels}


def sgr_bytes(units, mode, palette_index=None):
    """Bytes of a half-block picture with SGR changes only where needed."""
    rows, columns, _ = units.shape
    codes = np.round(srgb_encode(units) * 255).astype(int)
    total = 0
    for row in range(0, rows, 2):
        previous = (None, None)
        for column in range(columns):
            pair = []
            for r in (row, row + 1):
                if mode == 'direct':
                    pair.append(tuple(codes[r, column]))
                else:
                    pair.append(int(palette_index[r, column]))
            sgr = []
            for value, old, base in ((pair[0], previous[0], 38), (pair[1], previous[1], 48)):
                if value != old:
                    sgr.append(f'{base};2;{value[0]};{value[1]};{value[2]}' if mode == 'direct'
                               else f'{base};5;{value}')
            if sgr:
                total += len(f'\x1b[{";".join(sgr)}m')
            total += len('▀'.encode())
            previous = tuple(pair)
        total += len('\x1b[0m\r\n')
    return total


@functools.lru_cache(maxsize=1)
def palette_study():
    columns, rows = HALF_BLOCK['columns'], HALF_BLOCK['rows']
    sources = [('Snake', image_units(TILE_DEMO['image'], columns, 2 * rows)),
               ('Sunset gradient', image_units_from_array(sunset_gradient(1200, 900),
                                                          columns, 2 * rows))]
    results, panels = [], []
    for name, units in sources:
        direct = round_bits(units, 8)
        palette = adaptive_palette(direct, 256).colors
        palette_lab = linear_to_oklab(palette)
        index = np.argmin(((linear_to_oklab(direct)[..., None, :] - palette_lab) ** 2).sum(-1),
                          axis=-1)
        indexed = palette[index]
        delta = np.linalg.norm(linear_to_lab(direct) - linear_to_lab(indexed), axis=-1)
        codes = np.round(srgb_encode(direct) * 255).astype(int).reshape(-1, 3)
        reduced = np.round(srgb_encode(direct) * ((1 << HALF_BLOCK['precision_bits']) - 1))
        palette_codes = np.round(srgb_encode(palette) * 255).astype(int)
        upload = sum(len(f'\x1b]4;{i};rgb:{r:02x}/{g:02x}/{b:02x}\x1b\\')
                     for i, (r, g, b) in enumerate(palette_codes))
        results.append({
            'image': name, 'cells': columns * rows, 'color_units': columns * rows * 2,
            'distinct_8_bit': int(len(np.unique(codes, axis=0))),
            f'distinct_{HALF_BLOCK["precision_bits"]}_bit': int(len(np.unique(
                reduced.reshape(-1, 3), axis=0))),
            'delta_e_mean': round(float(delta.mean()), 2),
            'delta_e_p99': round(float(np.percentile(delta, 99)), 2),
            'bytes_direct': sgr_bytes(direct, 'direct'),
            'bytes_indexed': sgr_bytes(direct, 'indexed', index),
            'bytes_palette_upload': upload,
        })
        window = np.array([[delta[y:y + 20, x:x + 30].mean() for x in range(columns - 30)]
                           for y in range(2 * rows - 20)])
        y0, x0 = np.unravel_index(np.argmax(window), window.shape)
        panels.append((name, direct, indexed, (int(y0), int(x0))))
    return results, panels


def image_units_from_array(linear, columns, rows):
    planes = [np.asarray(Image.fromarray(linear[..., k].astype(np.float32), 'F')
                         .resize((columns, rows), Image.Resampling.BOX)) for k in range(3)]
    return np.clip(np.stack(planes, axis=-1).astype(float), 0, 1)


def png_data_uri(linear_image):
    pixels = np.round(srgb_encode(linear_image) * 255).astype(np.uint8)
    buffer = io.BytesIO()
    Image.fromarray(pixels, 'RGB').save(buffer, format='PNG', optimize=True)
    return 'data:image/png;base64,' + base64.b64encode(buffer.getvalue()).decode('ascii')


def svg_image(x, y, width, height, linear_image):
    return (f'<image x="{x}" y="{y}" width="{width}" height="{height}" '
            'preserveAspectRatio="none" image-rendering="optimizeSpeed" '
            f'href="{png_data_uri(linear_image)}"/>\n'
            f'<rect x="{x}" y="{y}" width="{width}" height="{height}" fill="none" stroke="{GRID}"/>')


def tile_dithering():
    study = tile_study()
    width, height = TILE_DEMO['size']
    tile = TILE_DEMO['tile']
    colors = 1 << (3 * TILE_DEMO['figure_bits'])
    gain = next(r for r in study['gains'] if r['image'] == 'snake'
                and r['palette'] == f'uniform {TILE_DEMO["figure_bits"]} bits per channel')
    gap, left = 30, 15
    body = []
    rows = ((f'One color per pixel, {colors} uniform colors', study['panels']['fine'], 40,
             ('', f'visible error {gain["pixel_dithered"]:.1f}',
              f'visible error {gain["pixel_none"]:.1f}')),
            (f'One color per {tile} × {tile} tile, the same {colors} colors',
             study['panels']['tile'], 410,
             ('', f'visible error {gain["tile_dithered"]:.1f}',
              f'visible error {gain["tile_none"]:.1f}')))
    for title, panels, top, notes in rows:
        body.append(svg_text(left, top - 12, title, 17, INK, 700))
        for column, ((label, image), note) in enumerate(zip(panels, notes)):
            x = left + column * (width + gap)
            body.append(svg_image(x, top, width, height, image))
            body.append(svg_text(x, top + height + 22, label, 15, INK, 700))
            if note:
                body.append(svg_text(x, top + height + 42, note, 14, MUTED))
    return svg_document(2 * left + 3 * width + 2 * gap, 735,
                        'Dithering on fine pixels and on coarse tiles',
                        f'Top row: a photograph of a snake as the original, and the same '
                        f'photograph with {colors} uniformly spaced colors, dithered and not '
                        'dithered, one color per pixel. Bottom row: the photograph with one '
                        'color per tile, as the 24-bit mean of each tile, and with the same '
                        'colors dithered between tiles and not dithered. Dithering helps on '
                        'the fine grid and hurts on the tiles.',
                        body)


def palette_direct():
    _, panels = palette_study()
    scale, zoom = 3, 9
    crop_w, crop_h = 30, 20
    left, gap = 15, 20
    full_w, full_h = HALF_BLOCK['columns'] * scale, HALF_BLOCK['rows'] * 2 * scale
    body = []
    top = 40
    for name, direct, indexed, (y0, x0) in panels:
        body.append(svg_text(left, top - 12, name, 17, INK, 700))
        items = [('24-bit direct color', direct, full_w, full_h, scale),
                 ('256 colors redefined with OSC 4', indexed, full_w, full_h, scale),
                 ('Enlarged, 24-bit', direct[y0:y0 + crop_h, x0:x0 + crop_w],
                  crop_w * zoom, crop_h * zoom, zoom),
                 ('Enlarged, 256 colors', indexed[y0:y0 + crop_h, x0:x0 + crop_w],
                  crop_w * zoom, crop_h * zoom, zoom)]
        x = left
        for label, image, w, h, factor in items:
            body.append(svg_image(x, top, w, h, expand(image, factor)))
            body.append(svg_text(x, top + full_h + 22, label, 15, INK, 700))
            if factor == scale:
                body.append(f'<rect x="{x + x0 * scale}" y="{top + y0 * scale}" '
                            f'width="{crop_w * scale}" height="{crop_h * scale}" '
                            'fill="none" stroke="#00b7c7" stroke-width="2"/>')
            x += w + gap
        top += full_h + 75
    width = left * 2 + 2 * full_w + 2 * crop_w * zoom + 3 * gap
    return svg_document(width, top - 25, '24-bit direct color and a 256-color palette',
                        'A photograph of a snake and a synthetic sunset, each drawn as '
                        f'{HALF_BLOCK["columns"]} by {HALF_BLOCK["rows"]} half-block cells, '
                        'once with 24-bit direct color and once with 256 palette colors '
                        'chosen for the image and set with OSC 4, without dithering, with an '
                        'enlarged region of each where they differ most.',
                        body)


FIGURES = {
    'csf-ratio.svg': csf_ratio,
    'tile-dithering.svg': tile_dithering,
    'palette-direct.svg': palette_direct,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {name: draw() for name, draw in FIGURES.items()}
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_character_cell_figures.py',
        'plot_dependency': 'numpy, Pillow',
        'generated': sorted(FIGURES),
        'data_sources': {
            'csf-ratio.svg': 'Mannos and Sakrison (1974) luminance model and schematic '
                             'chroma curve after Mullen (1985), as in the visual perception '
                             'chapter; cell bands from character_cell_examples',
            'tile-dithering.svg': 'images/snake.png; 512 uniformly spaced colors; averages and '
                                  'Floyd-Steinberg error in linear light; nearest colors '
                                  'by DIN99d',
            'palette-direct.svg': 'images/snake.png and a synthetic sunset; 256 colors by '
                                  'median cut refined with k-means in OKLab',
        },
        'contrast_sensitivity_samples': csf_samples(),
        'character_cell_examples': character_cell_examples(),
        'tile_study': {'viewing': {'ppi': TILE_DEMO['ppi'],
                                   'distance_mm': TILE_DEMO['distance_mm'],
                                   'pixels_per_degree': tile_study()['ppd']},
                       'tile_px': TILE_DEMO['tile'],
                       'dithering_gain': tile_study()['gains'],
                       'tile_precision': tile_study()['precision']},
        'palette_study': {'half_block_cells': [HALF_BLOCK['columns'], HALF_BLOCK['rows']],
                          'results': palette_study()[0]},
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
