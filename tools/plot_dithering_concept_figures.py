#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the dithering concept figures.

Use Matplotlib 3.10, NumPy and Pillow.  Run without arguments to regenerate
the figures under docs/concepts/dithering/figures, or with --check to verify
that the committed files are current.  Every figure is computed here from
first principles; no libsixel output is involved, so the figures illustrate
the methods rather than measure an implementation.
"""

from __future__ import annotations

import argparse
import html
import io
import json
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/concepts/dithering/figures'
INK = '#183247'
MUTED = '#4c6070'
GRID = '#d9e1e7'
ACCENT = '#c0392b'
BLUE = '#2f5fb3'
NS = 'http://www.w3.org/2000/svg'
RC = {'svg.fonttype': 'none', 'svg.hashsalt': 'libsixel-dithering-concepts',
      'font.family': 'sans-serif', 'font.sans-serif': ['DejaVu Sans', 'Arial'],
      'font.size': 13, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
      'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.titlesize': 13,
      'axes.titlecolor': INK}
SEED = 20261006


def svg_text(x, y, value, size=16, color=INK, weight=400, anchor='start'):
    return (f'<text x="{x:g}" y="{y:g}" font-size="{size}" fill="{color}" '
            f'font-weight="{weight}" text-anchor="{anchor}">'
            f'{html.escape(value)}</text>')


def svg_document(width, height, title, description, body):
    return (f'<svg xmlns="{NS}" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}" role="img" '
            'aria-labelledby="title desc" font-family="Arial, sans-serif">\n'
            f'<title id="title">{html.escape(title)}</title>\n'
            f'<desc id="desc">{html.escape(description)}</desc>\n'
            f'<rect x="0" y="0" width="{width}" height="{height}" fill="#ffffff"/>\n'
            + '\n'.join(body) + '\n</svg>\n')


def mpl_svg(fig):
    buffer = io.StringIO()
    fig.savefig(buffer, format='svg', metadata={'Date': None, 'Creator': None},
                bbox_inches='tight', pad_inches=0.15, facecolor='white')
    plt.close(fig)
    return buffer.getvalue()


def bare(ax):
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)


# --------------------------------------------------------------------------
# Dithering methods on a gray image in [0, 1], producing levels 0 and 1


def nearest(image, levels=2):
    return np.round(image * (levels - 1)) / (levels - 1)


def random_dither(image):
    rng = np.random.default_rng(SEED)
    return (image > rng.random(image.shape)).astype(float)


def bayer_matrix(order):
    matrix = np.array([[0, 2], [3, 1]])
    while matrix.shape[0] < order:
        matrix = np.block([[4 * matrix, 4 * matrix + 2], [4 * matrix + 3, 4 * matrix + 1]])
    return matrix


def ordered_dither(image, order=8):
    matrix = bayer_matrix(order)
    threshold = (matrix + 0.5) / matrix.size
    height, width = image.shape
    tiled = np.tile(threshold, (height // order + 1, width // order + 1))[:height, :width]
    return (image > tiled).astype(float)


def error_diffusion(image, levels=2):
    work = image.astype(float).copy()
    out = np.zeros_like(work)
    height, width = work.shape
    for y in range(height):
        for x in range(width):
            old = work[y, x]
            new = np.round(np.clip(old, 0, 1) * (levels - 1)) / (levels - 1)
            out[y, x] = new
            error = old - new
            if x + 1 < width:
                work[y, x + 1] += error * 7 / 16
            if y + 1 < height:
                if x > 0:
                    work[y + 1, x - 1] += error * 3 / 16
                work[y + 1, x] += error * 5 / 16
                if x + 1 < width:
                    work[y + 1, x + 1] += error * 1 / 16
    return out


def void_and_cluster(size=128, sigma=1.5):
    """Blue-noise threshold array by Ulichney's void-and-cluster method."""
    rng = np.random.default_rng(SEED)
    freq = np.fft.fftfreq(size)
    fx, fy = np.meshgrid(freq, freq)
    kernel = np.exp(-2 * (np.pi * sigma) ** 2 * (fx ** 2 + fy ** 2))

    def energy(pattern):
        return np.real(np.fft.ifft2(np.fft.fft2(pattern) * kernel))

    pattern = np.zeros((size, size), bool)
    initial = size * size // 10
    pattern.flat[rng.choice(size * size, initial, replace=False)] = True
    # Phase 0: move points from the tightest cluster to the largest void.
    for _ in range(size * size):
        field = energy(pattern)
        cluster = np.argmax(np.where(pattern, field, -np.inf))
        pattern.flat[cluster] = False
        field = energy(pattern)
        void = np.argmin(np.where(pattern, np.inf, field))
        if void == cluster:
            pattern.flat[cluster] = True
            break
        pattern.flat[void] = True
    ranks = np.zeros(size * size, int)
    prototype = pattern.copy()
    # Phase 1: remove points from tightest clusters, ranking downward.
    work = prototype.copy()
    for rank in range(initial - 1, -1, -1):
        field = energy(work)
        cluster = np.argmax(np.where(work, field, -np.inf))
        work.flat[cluster] = False
        ranks[cluster] = rank
    # Phases 2 and 3: add points to the largest voids, ranking upward.
    work = prototype.copy()
    for rank in range(initial, size * size):
        field = energy(work)
        void = np.argmin(np.where(work, np.inf, field))
        work.flat[void] = True
        ranks[void] = rank
    return ranks.reshape(size, size)


BLUE_NOISE = None


def blue_noise_dither(image):
    global BLUE_NOISE
    if BLUE_NOISE is None:
        BLUE_NOISE = void_and_cluster()
    size = BLUE_NOISE.shape[0]
    threshold = (BLUE_NOISE + 0.5) / BLUE_NOISE.size
    height, width = image.shape
    tiled = np.tile(threshold, (height // size + 1, width // size + 1))[:height, :width]
    return (image > tiled).astype(float)


def gaussian_blur(image, sigma):
    radius = int(3 * sigma)
    kernel = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    padded = np.pad(image, radius, mode='reflect')
    rows = np.apply_along_axis(lambda r: np.convolve(r, kernel, mode='valid'), 1, padded)
    return np.apply_along_axis(lambda c: np.convolve(c, kernel, mode='valid'), 0, rows)


METHODS = (('Random (white noise)', random_dither),
           ('Ordered (Bayer 8 × 8)', ordered_dither),
           ('Error diffusion (Floyd–Steinberg)', error_diffusion),
           ('Blue-noise mask (void-and-cluster)', blue_noise_dither))


# --------------------------------------------------------------------------
# Figures


def posterization():
    ramp = np.tile(np.linspace(0, 1, 320), (40, 1))
    source = Image.open(ROOT / 'images/snake.png').convert('RGB')
    source = source.resize((240, 180), Image.Resampling.LANCZOS)
    palette = source.quantize(colors=8, method=Image.Quantize.MEDIANCUT)
    flat = source.quantize(palette=palette, dither=Image.Dither.NONE).convert('RGB')
    dithered = source.quantize(palette=palette,
                               dither=Image.Dither.FLOYDSTEINBERG).convert('RGB')
    with plt.rc_context(RC):
        fig = plt.figure(figsize=(7.2, 4.6))
        grid = fig.add_gridspec(4, 3, height_ratios=[1, 1, 1, 4.2], hspace=0.55,
                                wspace=0.05)
        rows = (('Original gradient', ramp),
                ('Nearest of 4 grays: visible steps', nearest(ramp, 4)),
                ('Error diffusion to the same 4 grays', error_diffusion(ramp, 4)))
        for index, (label, image) in enumerate(rows):
            ax = fig.add_subplot(grid[index, :])
            ax.imshow(image, cmap='gray', vmin=0, vmax=1, interpolation='none',
                      aspect='auto')
            ax.set_title(label, fontsize=11, loc='left')
            bare(ax)
            for spine in ax.spines.values():
                spine.set_visible(True)
                spine.set_color(GRID)
        for column, (label, image) in enumerate((('Original', source),
                                                 ('8 colors, nearest', flat),
                                                 ('8 colors, error diffusion', dithered))):
            ax = fig.add_subplot(grid[3, column])
            ax.imshow(image, interpolation='none')
            ax.set_title(label, fontsize=11)
            bare(ax)
        return mpl_svg(fig)


def methods_comparison():
    ramp = np.tile(np.linspace(0, 1, 192), (40, 1))
    with plt.rc_context(RC):
        fig, axes = plt.subplots(len(METHODS), 2, figsize=(7.2, 5.0),
                                 gridspec_kw={'hspace': 0.7, 'wspace': 0.06})
        for row, (label, method) in enumerate(METHODS):
            pattern = method(ramp)
            for column, image in enumerate((pattern, gaussian_blur(pattern, 2.0))):
                ax = axes[row][column]
                ax.imshow(image, cmap='gray', vmin=0, vmax=1, interpolation='none',
                          aspect='auto')
                bare(ax)
            axes[row][0].set_title(label, fontsize=11, loc='left')
        axes[0][0].text(0.5, 1.75, 'Up close', transform=axes[0][0].transAxes,
                        ha='center', fontsize=13, fontweight='bold', color=INK)
        axes[0][1].text(0.5, 1.75, 'From a distance (blurred)',
                        transform=axes[0][1].transAxes, ha='center', fontsize=13,
                        fontweight='bold', color=INK)
        return mpl_svg(fig)


def radial_profile(power):
    size = power.shape[0]
    freq = np.fft.fftshift(np.fft.fftfreq(size))
    fx, fy = np.meshgrid(freq, freq)
    radius = np.hypot(fx, fy)
    bins = np.linspace(0, 0.5, 33)
    centers = (bins[:-1] + bins[1:]) / 2
    values = [power[(radius >= lo) & (radius < hi)].mean() for lo, hi in zip(bins[:-1], bins[1:])]
    return centers, np.array(values)


def spectra():
    size = 128
    gray = np.full((size, size), 0.3)
    colors = ('#7f8c8d', '#d08a00', ACCENT, BLUE)
    with plt.rc_context(RC):
        fig = plt.figure(figsize=(7.2, 5.4))
        grid = fig.add_gridspec(2, 4, height_ratios=[1, 1.15], hspace=0.45, wspace=0.08)
        curve_ax = fig.add_subplot(grid[1, :])
        for index, ((label, method), color) in enumerate(zip(METHODS, colors)):
            pattern = method(gray)
            centered = pattern - pattern.mean()
            power = np.abs(np.fft.fftshift(np.fft.fft2(centered))) ** 2 / centered.size
            # Widen isolated spectral lines so that single-bin peaks stay visible.
            shown = power.copy()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    shown = np.maximum(shown, np.roll(np.roll(power, dy, 0), dx, 1))
            ax = fig.add_subplot(grid[0, index])
            ax.imshow(np.log10(shown + 1e-3), cmap='magma', interpolation='none',
                      vmin=-3, vmax=1.5)
            ax.set_title(label.split(' (')[0], fontsize=11)
            bare(ax)
            centers, profile = radial_profile(power)
            curve_ax.semilogy(centers, profile + 1e-4, color=color, lw=2.2, label=label)
        curve_ax.set_xlim(0, 0.5)
        curve_ax.set_ylim(1e-4, 10)
        curve_ax.set_yticks([1e-4, 1e-2, 1])
        curve_ax.set_yticklabels(['0.0001', '0.01', '1'])
        curve_ax.minorticks_off()
        curve_ax.set_xlabel('Spatial frequency (cycles per pixel): coarse → fine')
        curve_ax.set_ylabel('Noise power (log scale)')
        curve_ax.set_title('Where each method puts its error', fontsize=13,
                           fontweight='bold')
        curve_ax.grid(color=GRID, lw=0.8)
        curve_ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.22), ncol=2,
                        fontsize=10.5, frameon=False)
        fig.text(0.5, 0.97, 'Spectra of a flat 30% gray dithered to black and white',
                 ha='center', fontsize=13, fontweight='bold', color=INK)
        return mpl_svg(fig)


def bayer_figure():
    matrix = bayer_matrix(4)
    gray = 0.4
    width, height = 720, 270
    body = []
    cell = 40
    x0, y0 = 60, 50
    body.append(svg_text(x0 + 2 * cell, 32, 'Threshold matrix', 16, INK, 700, 'middle'))
    for y in range(4):
        for x in range(4):
            value = int(matrix[y, x])
            shade = int(round((value + 0.5) / 16 * 255))
            fill = '#%02x%02x%02x' % (shade, shade, shade)
            text_color = '#ffffff' if shade < 128 else INK
            body.append(f'<rect x="{x0 + x * cell}" y="{y0 + y * cell}" width="{cell}" '
                        f'height="{cell}" fill="{fill}" stroke="#ffffff"/>')
            body.append(svg_text(x0 + x * cell + cell / 2, y0 + y * cell + 26, str(value),
                                 15, text_color, 700, 'middle'))
    body.append(svg_text(x0 + 2 * cell, y0 + 4 * cell + 26, 'threshold = (v + 0.5) / 16',
                         13, MUTED, 400, 'middle'))
    # Input gray and output pattern
    x1 = 285
    body.append(svg_text(x1 + 2 * cell, 32, f'Input gray {gray:.1f}', 16, INK, 700, 'middle'))
    shade = int(round(gray * 255))
    body.append(f'<rect x="{x1}" y="{y0}" width="{4 * cell}" height="{4 * cell}" '
                f'fill="#{shade:02x}{shade:02x}{shade:02x}"/>')
    x2 = 510
    body.append(svg_text(x2 + 2 * cell, 32, 'Output tile', 16, INK, 700, 'middle'))
    body.append(svg_text(x2 + 2 * cell, y0 + 4 * cell + 26, 'white where gray > threshold',
                         13, MUTED, 400, 'middle'))
    threshold = (matrix + 0.5) / 16
    for y in range(4):
        for x in range(4):
            fill = '#ffffff' if gray > threshold[y, x] else '#000000'
            body.append(f'<rect x="{x2 + x * cell}" y="{y0 + y * cell}" width="{cell}" '
                        f'height="{cell}" fill="{fill}" stroke="{MUTED}" stroke-width="0.5"/>')
    for xa, xb in ((x0 + 4 * cell + 12, x1 - 12), (x1 + 4 * cell + 12, x2 - 12)):
        body.append(f'<path d="M {xa} {y0 + 2 * cell} L {xb} {y0 + 2 * cell}" '
                    f'stroke="{MUTED}" stroke-width="2.5"/>')
        body.append(f'<path d="M {xb - 8} {y0 + 2 * cell - 6} L {xb} {y0 + 2 * cell} '
                    f'L {xb - 8} {y0 + 2 * cell + 6}" fill="none" stroke="{MUTED}" '
                    'stroke-width="2.5"/>')
    white = int((gray > threshold).sum())
    body.append(svg_text(width / 2, 258, f'{white} of 16 cells are white: the tile '
                         f'averages {white / 16:.3f}, close to the input {gray:.1f}.',
                         14, MUTED, 400, 'middle'))
    return svg_document(width, height, 'Ordered dither',
                        'A four by four Bayer threshold matrix, a uniform input gray of '
                        '0.4, and the resulting black and white tile in which six of '
                        'sixteen cells are white.', body)


def kernel_figure():
    width, height = 720, 250
    cell = 64
    x0, y0 = 220, 50
    body = []
    labels = {(0, 0): ('done', '#e9eef2'), (1, 0): ('done', '#e9eef2'),
              (2, 0): ('done', '#e9eef2'), (0, 1): ('done', '#e9eef2'),
              (1, 1): ('current', '#f4d6d2'), (2, 1): ('7/16', '#ffffff'),
              (0, 2): ('3/16', '#ffffff'), (1, 2): ('5/16', '#ffffff'),
              (2, 2): ('1/16', '#ffffff')}
    for (x, y), (label, fill) in labels.items():
        body.append(f'<rect x="{x0 + x * cell}" y="{y0 + y * cell - 40}" width="{cell}" '
                    f'height="{cell}" fill="{fill}" stroke="{MUTED}"/>')
        color = ACCENT if label == 'current' else (MUTED if label == 'done' else INK)
        weight = 700 if '/' in label or label == 'current' else 400
        body.append(svg_text(x0 + x * cell + cell / 2, y0 + y * cell - 40 + 38, label,
                             15, color, weight, 'middle'))
    body.append(f'<path d="M 40 {y0 + 1 * cell - 8} L 180 {y0 + 1 * cell - 8}" '
                f'stroke="{MUTED}" stroke-width="2.5"/>')
    body.append(f'<path d="M 172 {y0 + cell - 14} L 180 {y0 + cell - 8} L 172 '
                f'{y0 + cell - 2}" fill="none" stroke="{MUTED}" stroke-width="2.5"/>')
    body.append(svg_text(40, y0 + cell - 18, 'scan order', 14, MUTED))
    notes = ('1. Choose the nearest palette color',
             '   for the current pixel.',
             '2. Error = wanted − chosen.',
             '3. Add the error, split by these',
             '   weights, to pixels not yet drawn.')
    for index, line in enumerate(notes):
        body.append(svg_text(445, 40 + index * 26, line, 15, INK))
    body.append(svg_text(width / 2, 238, 'The weights sum to 1, so the average tone of '
                         'each neighborhood is preserved.', 14, MUTED, 400, 'middle'))
    return svg_document(width, height, 'Floyd-Steinberg error diffusion',
                        'A three by three neighborhood. Pixels above and to the left are '
                        'done. The current pixel passes its quantization error to the '
                        'right neighbor with weight 7/16 and to the three pixels below '
                        'with weights 3/16, 5/16 and 1/16.', body)


def linear_averaging():
    size = 96
    checker = (np.indices((size, size)).sum(axis=0) % 2).astype(float)
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 3, figsize=(7.2, 2.7), gridspec_kw={'wspace': 0.08})
        panels = (('sRGB 128\n(half the code value)', np.full((size, size), 128 / 255)),
                  ('Black and white\ncheckerboard', checker),
                  ('sRGB 188\n(half the light)', np.full((size, size), 188 / 255)))
        for ax, (label, image) in zip(axes, panels):
            ax.imshow(image, cmap='gray', vmin=0, vmax=1, interpolation='none')
            ax.set_title(label, fontsize=11)
            bare(ax)
        return mpl_svg(fig)


FIGURES = {
    'posterization.svg': posterization,
    'methods-comparison.svg': methods_comparison,
    'spectra.svg': spectra,
    'ordered-dither.svg': bayer_figure,
    'error-diffusion-kernel.svg': kernel_figure,
    'linear-averaging.svg': linear_averaging,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {name: draw() for name, draw in FIGURES.items()}
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_dithering_concept_figures.py',
        'plot_dependency': 'matplotlib 3.10, numpy, Pillow',
        'seed': SEED,
        'methods': {
            'random': 'uniform random threshold per pixel',
            'ordered': 'recursive Bayer threshold matrix, thresholds (v + 0.5) / n',
            'error_diffusion': 'Floyd-Steinberg weights 7/16, 3/16, 5/16, 1/16, raster scan',
            'blue_noise': 'void-and-cluster rank array, 128 x 128, Gaussian sigma 1.5',
        },
        'posterization_palette': 'Pillow median cut, 8 colors, images/snake.png',
        'figures': sorted(FIGURES),
    }, indent=2) + '\n'
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
