#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the visual-perception explanatory figures.

Use Matplotlib 3.10, NumPy and Pillow.  Run without arguments to regenerate
the figures under docs/quality/visual-perception/figures, or with --check to
verify that the committed files are current.

Data sources are named in each figure and in figures.json.  Curves that are
schematic say so in their labels; tabulated CIE functions are reproduced from
the published 10 nm tables.
"""

from __future__ import annotations

import argparse
import base64
import functools
import html
import io
import json
import math
import textwrap
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/quality/visual-perception/figures'
INK = '#183247'
MUTED = '#4c6070'
GRID = '#d9e1e7'
L_COLOR = '#c0392b'
M_COLOR = '#2e8b57'
S_COLOR = '#2f5fb3'
ROD_COLOR = '#555555'
NS = 'http://www.w3.org/2000/svg'
RC = {'svg.fonttype': 'none', 'svg.hashsalt': 'libsixel-visual-perception',
      'font.family': 'sans-serif', 'font.sans-serif': ['DejaVu Sans', 'Arial'],
      'font.size': 13, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
      'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.titlesize': 14,
      'axes.titleweight': 'bold', 'axes.titlecolor': INK}

# CIE 1924 photopic V(lambda) and CIE 1951 scotopic V'(lambda), 380-780 nm.
WAVELENGTHS = list(range(380, 781, 10))
V_PHOTOPIC = [
    0.000039, 0.00012, 0.000396, 0.00121, 0.004, 0.0116, 0.023, 0.038,
    0.06, 0.09098, 0.13902, 0.20802, 0.323, 0.503, 0.71, 0.862, 0.954,
    0.99495, 0.995, 0.952, 0.87, 0.757, 0.631, 0.503, 0.381, 0.265, 0.175,
    0.107, 0.061, 0.032, 0.017, 0.00821, 0.004102, 0.002091, 0.001047,
    0.00052, 0.000249, 0.00012, 0.00006, 0.00003, 0.000015]
V_SCOTOPIC = [
    0.000589, 0.002209, 0.00929, 0.03484, 0.0966, 0.1998, 0.3281, 0.455,
    0.567, 0.676, 0.793, 0.904, 0.982, 0.997, 0.935, 0.811, 0.65, 0.481,
    0.3288, 0.2076, 0.1212, 0.0655, 0.03315, 0.01593, 0.00737, 0.003335,
    0.001497, 0.000677, 0.0003129, 0.000148, 0.0000715, 0.00003533,
    0.0000178, 0.00000914, 0.00000478, 0.000002546, 0.000001379,
    0.00000076, 0.000000425, 0.000000241, 0.000000139]


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


def arrow_defs():
    return ('<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" '
            'markerWidth="7" markerHeight="7" orient="auto">'
            f'<path d="M 0 0 L 10 5 L 0 10 z" fill="{MUTED}"/></marker></defs>')


def finish(fig, ax, note, ncol=2):
    """Put the legend and the source note below the axes."""
    handles, labels = ax.get_legend_handles_labels()
    y = -0.2
    if handles:
        ax.legend(handles, labels, loc='upper center', bbox_to_anchor=(0.5, y),
                  ncol=ncol, fontsize=11, frameon=False)
        y -= 0.085 * math.ceil(len(handles) / ncol) + 0.03
    if note:
        fig.text(0.0, y, textwrap.fill(note, 92), fontsize=10, color=MUTED,
                 ha='left', va='top', transform=ax.transAxes)


def mpl_svg(fig):
    buffer = io.StringIO()
    fig.savefig(buffer, format='svg', metadata={'Date': None, 'Creator': None},
                bbox_inches='tight', pad_inches=0.15, facecolor='white')
    plt.close(fig)
    return buffer.getvalue()


def srgb_encode(linear):
    linear = np.clip(linear, 0.0, 1.0)
    return np.where(linear <= 0.0031308, 12.92 * linear,
                    1.055 * np.power(linear, 1 / 2.4) - 0.055)


def srgb_decode(encoded):
    encoded = np.asarray(encoded, dtype=float)
    return np.where(encoded <= 0.04045, encoded / 12.92,
                    np.power((encoded + 0.055) / 1.055, 2.4))


def hex_color(linear_rgb):
    code = np.round(srgb_encode(np.asarray(linear_rgb)) * 255).astype(int)
    return '#%02x%02x%02x' % tuple(code)


# --------------------------------------------------------------------------
# Data plots


def govardovskii(wavelength, peak):
    """A1 photopigment alpha-band template (Govardovskii et al., 2000)."""
    x = peak / wavelength
    a = 0.8795 + 0.0459 * np.exp(-((peak - 300.0) ** 2) / 11940.0)
    return 1.0 / (np.exp(69.7 * (a - x)) + np.exp(28.0 * (0.922 - x))
                  + np.exp(-14.9 * (1.104 - x)) + 0.674)


def cone_sensitivity():
    wavelength = np.linspace(380, 720, 341)
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(7.2, 3.9))
        for name, peak, color in (('S cones', 420, S_COLOR),
                                  ('Rods', 498, ROD_COLOR),
                                  ('M cones', 530, M_COLOR),
                                  ('L cones', 559, L_COLOR)):
            curve = govardovskii(wavelength, peak)
            style = '--' if name == 'Rods' else '-'
            ax.plot(wavelength, curve / curve.max(), style, color=color, lw=2.4,
                    label=f'{name} (peak ≈ {peak} nm)')
        spectrum = np.linspace(380, 720, 341)
        ax.imshow(np.array([wavelength_to_rgb(w) for w in spectrum])[None, :, :],
                  extent=(380, 720, -0.13, -0.04), aspect='auto', interpolation='bilinear')
        ax.set_xlim(380, 720)
        ax.set_ylim(-0.14, 1.05)
        ax.set_yticks([0, 0.5, 1])
        ax.set_xlabel('Wavelength (nm)')
        ax.set_ylabel('Relative sensitivity')
        ax.set_title('Each receptor type responds to a band of wavelengths')
        ax.grid(color=GRID, lw=0.8)
        finish(fig, ax, 'Photopigment templates (Govardovskii et al., 2000); the '
               'eye\'s lens and macular pigment further reduce short-wavelength light.')
        return mpl_svg(fig)


def wavelength_to_rgb(wavelength):
    """Approximate display color of a spectral wavelength, for the strip only."""
    if wavelength < 440:
        rgb = (-(wavelength - 440) / 60, 0.0, 1.0)
    elif wavelength < 490:
        rgb = (0.0, (wavelength - 440) / 50, 1.0)
    elif wavelength < 510:
        rgb = (0.0, 1.0, -(wavelength - 510) / 20)
    elif wavelength < 580:
        rgb = ((wavelength - 510) / 70, 1.0, 0.0)
    elif wavelength < 645:
        rgb = (1.0, -(wavelength - 645) / 65, 0.0)
    else:
        rgb = (1.0, 0.0, 0.0)
    fade = 1.0
    if wavelength < 420:
        fade = 0.3 + 0.7 * (wavelength - 380) / 40
    elif wavelength > 680:
        fade = 0.3 + 0.7 * (720 - wavelength) / 40
    return [max(0.0, c * fade) for c in rgb]


def luminosity_functions():
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(7.2, 3.6))
        ax.plot(WAVELENGTHS, V_PHOTOPIC, color='#d08a00', lw=2.6,
                label="Daylight (photopic) V(λ), peak 555 nm")
        ax.plot(WAVELENGTHS, V_SCOTOPIC, color=S_COLOR, lw=2.6, ls='--',
                label="Night (scotopic) V′(λ), peak 507 nm")
        ax.annotate('', xy=(507, 1.04), xytext=(555, 1.04),
                    arrowprops={'arrowstyle': '->', 'color': INK, 'lw': 1.5})
        ax.text(531, 1.07, 'Purkinje shift', ha='center', fontsize=12, color=INK)
        ax.set_xlim(380, 720)
        ax.set_ylim(0, 1.16)
        ax.set_yticks([0, 0.5, 1])
        ax.set_xlabel('Wavelength (nm)')
        ax.set_ylabel('Relative luminous efficiency')
        ax.set_title('In dim light the eye’s peak sensitivity moves toward blue-green')
        ax.grid(color=GRID, lw=0.8)
        finish(fig, ax, 'CIE 1924 photopic and CIE 1951 scotopic tables, 10 nm steps.',
               ncol=1)
        return mpl_svg(fig)


def receptor_density():
    eccentricity = np.linspace(-70, 70, 1401)
    e = np.abs(eccentricity)
    cones = 6000 + 190000 * np.exp(-e / 0.35) + 9000 * np.exp(-e / 5.0)
    rods = 160000 * (1 - np.exp(-e / 4.0)) * np.exp(-((e - 18.0) / 40.0) ** 2)
    blind = (eccentricity > 12.5) & (eccentricity < 18.0)
    cones = np.where(blind, 0, cones)
    rods = np.where(blind, 0, rods)
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(7.2, 3.9))
        ax.plot(eccentricity, rods / 1000, color=ROD_COLOR, lw=2.4, label='Rods')
        ax.plot(eccentricity, cones / 1000, color=L_COLOR, lw=2.4, label='Cones')
        ax.axvspan(12.5, 18.0, color='#e8edf1')
        ax.text(15.25, 182, 'Blind spot\n(optic disc)', ha='center', va='top',
                fontsize=11, color=INK)
        ax.annotate('Fovea: cones only', xy=(0, 196), xytext=(-46, 182),
                    fontsize=11, color=INK,
                    arrowprops={'arrowstyle': '->', 'color': INK, 'lw': 1.2})
        ax.set_xlim(-70, 70)
        ax.set_ylim(0, 210)
        ax.set_xlabel('Distance from the fovea (degrees): temporal ← → nasal')
        ax.set_ylabel('Receptors per mm² (thousands)')
        ax.set_title('Cones crowd the center; rods fill the periphery')
        ax.grid(color=GRID, lw=0.8)
        finish(fig, ax, 'Schematic curves shaped after Curcio et al. (1990), right eye.')
        return mpl_svg(fig)


def dark_adaptation():
    t = np.linspace(0, 40, 401)
    cone = 3.0 + 1.6 * np.exp(-t / 2.0)
    rod = 0.6 + 8.0 * np.exp(-t / 7.0)
    threshold = np.minimum(cone, rod)
    break_t = t[np.argmax(rod < cone)]
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(7.2, 3.7))
        ax.plot(t, cone, color=L_COLOR, lw=1.4, ls=':', alpha=0.8)
        ax.plot(t, rod, color=ROD_COLOR, lw=1.4, ls=':', alpha=0.8)
        ax.plot(t, threshold, color=INK, lw=2.8)
        ax.annotate('Cones reach their limit\nafter about 10 minutes', xy=(6, 3.07),
                    xytext=(13, 4.4), fontsize=11, color=L_COLOR,
                    arrowprops={'arrowstyle': '->', 'color': L_COLOR, 'lw': 1.2})
        ax.annotate('Rod–cone break', xy=(break_t, 3.0), xytext=(break_t + 6, 2.2),
                    fontsize=11, color=INK,
                    arrowprops={'arrowstyle': '->', 'color': INK, 'lw': 1.2})
        ax.annotate('Rods keep improving\nfor 30–40 minutes', xy=(30, 0.88),
                    xytext=(22, 1.6), fontsize=11, color=ROD_COLOR,
                    arrowprops={'arrowstyle': '->', 'color': ROD_COLOR, 'lw': 1.2})
        ax.set_xlim(0, 40)
        ax.set_ylim(0, 5.2)
        ax.set_yticks([])
        ax.set_xlabel('Time in the dark (minutes)')
        ax.set_ylabel('Dimmest visible light\n(lower = more sensitive)')
        ax.set_title('Dark adaptation is slow: two receptor systems, two stages')
        ax.grid(color=GRID, lw=0.8, axis='x')
        finish(fig, ax, 'Schematic threshold curve; times are typical values.')
        return mpl_svg(fig)


CSF_GRID = np.logspace(-1, np.log10(60), 400)


def luminance_csf(f):
    """Mannos and Sakrison (1974) model, scaled to its peak on CSF_GRID."""
    def raw(x):
        return 2.6 * (0.0192 + 0.114 * x) * np.exp(-(0.114 * x) ** 1.1)
    return raw(np.asarray(f, dtype=float)) / raw(CSF_GRID).max()


def chroma_csf(f):
    """Schematic chroma low-pass after Mullen (1985), scaled to its peak."""
    return 1.0 / (1.0 + (np.asarray(f, dtype=float) / 1.5) ** 2.2)


# Frequencies tabulated in figures.json for the comparison of luminance and
# chroma sensitivity in the visual perception guide.
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


def contrast_sensitivity():
    f = CSF_GRID
    lum = luminance_csf(f)
    chroma = chroma_csf(f)
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(7.2, 3.9))
        ax.loglog(f, lum, color=INK, lw=2.6, label='Light–dark (luminance)')
        ax.loglog(f, chroma, color=L_COLOR, lw=2.6, ls='--',
                  label='Red–green and blue–yellow (chroma)')
        ax.axvspan(17, 30, color='#e3e9ee', label='One-pixel stripes on common displays')
        ax.set_xlim(0.1, 60)
        ax.set_ylim(0.0015, 1.6)
        ax.set_xticks([0.1, 1, 10])
        ax.set_xticklabels(['0.1', '1', '10'])
        ax.set_yticks([0.01, 0.1, 1])
        ax.set_yticklabels(['0.01', '0.1', '1'])
        ax.minorticks_off()
        ax.set_xlabel('Spatial frequency (cycles per degree of visual angle)')
        ax.set_ylabel('Relative sensitivity')
        ax.set_title('Fine detail is seen in light and dark, not in color')
        ax.grid(color=GRID, lw=0.8, which='major')
        finish(fig, ax, 'Luminance: Mannos and Sakrison (1974) model. Chroma: '
               'schematic after Mullen (1985). Each curve is scaled to its own peak.',
               ncol=1)
        return mpl_svg(fig)


# --------------------------------------------------------------------------
# Demonstrations


def campbell_robson():
    width, height = 900, 420
    x = np.linspace(0, 1, width)
    y = np.linspace(0, 1, height)[:, None]
    # Frequency rises exponentially from left to right and contrast falls
    # exponentially from bottom to top.
    phase = 2 * np.pi * 3.0 * (np.exp(x * np.log(60.0)) - 1) / np.log(60.0)
    contrast = np.exp(np.log(0.003) * (1 - y))
    # Gray levels are encoded display values, like the rest of the page.
    encoded = 0.5 + 0.5 * contrast * np.sin(phase)[None, :]
    with plt.rc_context(RC):
        fig, ax = plt.subplots(figsize=(7.2, 3.9))
        ax.imshow(encoded, cmap='gray', vmin=0, vmax=1, interpolation='none',
                  aspect='auto', extent=(0, 1, 0, 1))
        ax.set_xticks([0.02, 0.98])
        ax.set_xticklabels(['coarse', 'fine'])
        ax.set_yticks([0.03, 0.97])
        ax.set_yticklabels(['strong', 'faint'])
        ax.set_xlabel('Stripe spacing')
        ax.set_ylabel('Contrast')
        ax.set_title('Where do the stripes disappear?')
        return mpl_svg(fig)


def weber_demo():
    width, height = 720, 250
    body = []
    step = 0.02
    for index, (background, label) in enumerate(((0.02, 'Dark background'),
                                                  (0.60, 'Bright background'))):
        x0 = 30 + index * 350
        back = hex_color([background] * 3)
        patch = hex_color([background + step] * 3)
        body.append(f'<rect x="{x0}" y="50" width="310" height="150" fill="{back}"/>')
        body.append(f'<rect x="{x0 + 105}" y="85" width="100" height="80" fill="{patch}"/>')
        body.append(svg_text(x0 + 155, 35, label, 17, INK, 700, 'middle'))
        body.append(svg_text(x0 + 155, 228, f'background {background:.2f}, square '
                             f'{background + step:.2f} (linear light)', 13, MUTED, 400,
                             'middle'))
    return svg_document(width, height, 'Weber law demonstration',
                        'Two squares that are each 0.02 brighter, in linear light, than '
                        'their background. On the dark background the square is clearly '
                        'visible; on the bright background it is hard to see.', body)


def dither_integration():
    width, height = 256, 48
    ramp = np.tile(np.linspace(0, 1, width), (height, 1))
    rng = np.random.default_rng(20261006)
    random_dither = (ramp > rng.random(ramp.shape)).astype(float)
    diffused = floyd_steinberg(ramp)
    blur = gaussian_blur

    rows = [('Original gradient', ramp, ramp),
            ('Random dither: only black and white', random_dither, blur(random_dither, 2.5)),
            ('Error diffusion: only black and white', diffused, blur(diffused, 2.5))]
    with plt.rc_context(RC):
        fig, axes = plt.subplots(3, 2, figsize=(7.2, 3.6),
                                 gridspec_kw={'hspace': 0.75, 'wspace': 0.06})
        for row, (label, near, far) in enumerate(rows):
            for col, image in enumerate((near, far)):
                ax = axes[row][col]
                ax.imshow(image, cmap='gray', vmin=0, vmax=1, interpolation='none',
                          aspect='auto')
                ax.set_xticks([])
                ax.set_yticks([])
                for spine in ax.spines.values():
                    spine.set_visible(False)
            axes[row][0].set_title(label, fontsize=11, loc='left', fontweight='normal')
        axes[0][0].text(0.5, 1.9, 'Up close', transform=axes[0][0].transAxes,
                        ha='center', fontsize=13, fontweight='bold', color=INK)
        axes[0][1].text(0.5, 1.9, 'From a distance (blurred)',
                        transform=axes[0][1].transAxes, ha='center', fontsize=13,
                        fontweight='bold', color=INK)
        return mpl_svg(fig)


def floyd_steinberg(image):
    work = image.astype(float).copy()
    out = np.zeros_like(work)
    height, width = work.shape
    for y in range(height):
        for x in range(width):
            old = work[y, x]
            new = 1.0 if old >= 0.5 else 0.0
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


def gaussian_blur(image, sigma):
    radius = int(3 * sigma)
    kernel = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    padded = np.pad(image, radius, mode='reflect')
    rows = np.apply_along_axis(lambda r: np.convolve(r, kernel, mode='valid'), 1, padded)
    return np.apply_along_axis(lambda c: np.convolve(c, kernel, mode='valid'), 0, rows)


def luma_chroma_demo():
    source = Image.open(ROOT / 'images/snake.png').convert('RGB')
    source = source.resize((300, 225), Image.Resampling.LANCZOS)
    rgb = np.asarray(source, dtype=float) / 255.0
    # Encoded-value BT.601 YCbCr, as in JPEG.
    y = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]
    cb = (rgb[..., 2] - y) * 0.564
    cr = (rgb[..., 0] - y) * 0.713

    def to_rgb(yy, bb, rr):
        r = yy + 1.403 * rr
        b = yy + 1.773 * bb
        g = (yy - 0.299 * r - 0.114 * b) / 0.587
        return np.clip(np.stack([r, g, b], axis=-1), 0, 1)

    sigma = 4.0
    panels = [('Original', rgb),
              ('Color blurred', to_rgb(y, gaussian_blur(cb, sigma), gaussian_blur(cr, sigma))),
              ('Brightness blurred', to_rgb(gaussian_blur(y, sigma), cb, cr))]
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 3, figsize=(7.2, 2.35), gridspec_kw={'wspace': 0.05})
        for ax, (label, image) in zip(axes, panels):
            ax.imshow(image, interpolation='none')
            ax.set_title(label, fontsize=12)
            ax.set_xticks([])
            ax.set_yticks([])
        return mpl_svg(fig)


def chromatic_adaptation():
    patches = np.array([[0.70, 0.18, 0.12], [0.85, 0.62, 0.15], [0.20, 0.45, 0.18],
                        [0.15, 0.30, 0.65], [0.62, 0.62, 0.62], [0.30, 0.30, 0.30]])
    illuminant = np.array([1.0, 0.78, 0.48])
    seen = patches * illuminant
    adapted = seen / illuminant
    width, height = 720, 270
    body = []
    titles = ('Under daylight', 'Under warm lamplight', 'After adaptation')
    sets = (patches, seen, adapted)
    for column, (title, colors) in enumerate(zip(titles, sets)):
        x0 = 20 + column * 235
        body.append(svg_text(x0 + 105, 30, title, 16, INK, 700, 'middle'))
        for index, color in enumerate(colors):
            px = x0 + (index % 3) * 72
            py = 50 + (index // 3) * 80
            body.append(f'<rect x="{px}" y="{py}" width="64" height="70" rx="4" '
                        f'fill="{hex_color(color)}" stroke="#ffffff"/>')
        if column < 2:
            body.append(f'<path d="M {x0 + 218} 125 L {x0 + 232} 125" stroke="{MUTED}" '
                        'stroke-width="2.5" marker-end="url(#arrow)"/>')
    body.insert(0, arrow_defs())
    body.append(svg_text(360, 240, 'The light reaching the eye changes (middle); the visual',
                         14, MUTED, 400, 'middle'))
    body.append(svg_text(360, 260, 'system rescales each cone class and restores the colors (right).',
                         14, MUTED, 400, 'middle'))
    return svg_document(width, height, 'Chromatic adaptation',
                        'Six colored patches under daylight, the same patches tinted by '
                        'warm lamplight, and the colors restored after dividing each '
                        'channel by the illuminant, a simplified von Kries adaptation.',
                        body)


# --------------------------------------------------------------------------
# Diagrams


def opponent_channels():
    width, height = 720, 330
    body = [arrow_defs()]
    cones = (('L', 170, L_COLOR), ('M', 360, M_COLOR), ('S', 550, S_COLOR))
    for name, x, color in cones:
        body.append(f'<circle cx="{x}" cy="70" r="38" fill="{color}"/>')
        body.append(svg_text(x, 78, name, 24, '#ffffff', 700, 'middle'))
    body.append(svg_text(360, 20, 'Cone signals', 15, MUTED, 400, 'middle'))
    channels = (
        (140, 'Light–dark', 'L + M', ['#000000', '#ffffff']),
        (360, 'Red–green', 'L − M', ['#2e8b57', '#c0392b']),
        (580, 'Blue–yellow', 'S − (L + M)', ['#e3b505', '#2f5fb3']),
    )
    centers = {'L': 170, 'M': 360, 'S': 550}
    links = ((140, (('L', '+'), ('M', '+'))),
             (360, (('L', '+'), ('M', '−'))),
             (580, (('L', '−'), ('M', '−'), ('S', '+'))))
    for target, sources in links:
        spread = 46 if len(sources) == 2 else 52
        first = target - spread * (len(sources) - 1) / 2
        for index, (cone, sign) in enumerate(sources):
            x0 = centers[cone]
            x1 = first + index * spread
            body.append(f'<path d="M {x0} 110 L {x1:g} 184" stroke="{MUTED}" '
                        'stroke-width="1.6" marker-end="url(#arrow)"/>')
            mx, my = x0 + (x1 - x0) * 0.78, 110 + 74 * 0.78
            body.append(f'<circle cx="{mx:g}" cy="{my:g}" r="10" fill="#ffffff" '
                        f'stroke="{MUTED}"/>')
            body.append(svg_text(mx, my + 5, sign, 14, INK, 700, 'middle'))
    for x, title, formula, (left, right) in channels:
        body.append(f'<rect x="{x - 95}" y="190" width="190" height="70" rx="10" '
                    f'fill="#f4f7f9" stroke="{MUTED}"/>')
        body.append(svg_text(x, 216, title, 17, INK, 700, 'middle'))
        body.append(svg_text(x, 242, formula, 15, MUTED, 400, 'middle'))
        gradient_id = f'g{x}'
        body.append(f'<defs><linearGradient id="{gradient_id}"><stop offset="0" '
                    f'stop-color="{left}"/><stop offset="1" stop-color="{right}"/>'
                    '</linearGradient></defs>')
        body.append(f'<rect x="{x - 95}" y="275" width="190" height="22" '
                    f'fill="url(#{gradient_id})" stroke="{MUTED}"/>')
    body.append(svg_text(360, 320, 'Three opponent channels leave the eye. Only the '
                         'light–dark channel carries fine detail.', 14, MUTED, 400,
                         'middle'))
    return svg_document(width, height, 'Opponent channels',
                        'L, M and S cone signals combine into a light-dark channel, '
                        'L plus M; a red-green channel, L minus M; and a blue-yellow '
                        'channel, S minus L plus M.', body)


def light_levels():
    width, height = 760, 360
    left, right = 40, 720
    low, high = -6, 6

    def xpos(log_value):
        return left + (log_value - low) / (high - low) * (right - left)

    body = []
    regimes = ((-6, math.log10(0.005), 'Scotopic', '#dfe6f2', 'rods only'),
               (math.log10(0.005), math.log10(5), 'Mesopic', '#ece7f4', 'rods + cones'),
               (math.log10(5), 6, 'Photopic', '#f6eedc', 'cones'))
    for start, end, name, fill, note in regimes:
        body.append(f'<rect x="{xpos(start):g}" y="30" width="{xpos(end) - xpos(start):g}" '
                    f'height="110" fill="{fill}"/>')
        center = (xpos(start) + xpos(end)) / 2
        body.append(svg_text(center, 62, name, 18, INK, 700, 'middle'))
        body.append(svg_text(center, 84, note, 14, MUTED, 400, 'middle'))
    body.append(f'<line x1="{left}" y1="140" x2="{right}" y2="140" stroke="{INK}" '
                'stroke-width="2"/>')
    for value in range(low, high + 1, 2):
        x = xpos(value)
        body.append(f'<line x1="{x:g}" y1="140" x2="{x:g}" y2="148" stroke="{INK}"/>')
        if value == 0:
            label = svg_text(x, 166, '1', 13, MUTED, 400, 'middle')
        else:
            sign = '−' if value < 0 else ''
            label = (f'<text x="{x:g}" y="166" font-size="13" fill="{MUTED}" '
                     f'text-anchor="middle">10<tspan dy="-6" font-size="10">'
                     f'{sign}{abs(value)}</tspan></text>')
        body.append(label)
    examples = ((-3.5, 'Starlit night', 0), (-1.5, 'Full moon', 1),
                (-0.6, 'LCD black', 0), (2.0, 'Display white, office desk', 1),
                (3.7, 'Overcast sky', 0), (4.6, 'Sunlit snow', 1))
    for value, label, row in examples:
        x = xpos(value)
        y = 200 + row * 32
        body.append(f'<line x1="{x:g}" y1="140" x2="{x:g}" y2="{y - 15}" '
                    f'stroke="{MUTED}" stroke-dasharray="3 3"/>')
        body.append(f'<circle cx="{x:g}" cy="140" r="5" fill="{INK}"/>')
        body.append(svg_text(x, y, label, 14, INK, 400, 'middle'))
    body.append(svg_text((left + right) / 2, 290, 'Luminance in cd/m² on a logarithmic '
                         'scale: each tick is a factor of 100', 14, INK, 400, 'middle'))
    body.append(svg_text(left, 330, 'Approximate luminances. Regime boundaries follow '
                         'CIE 191:2010.', 12, MUTED))
    return svg_document(width, height, 'Light levels and vision regimes',
                        'A logarithmic luminance scale from 10 to the minus 6 to 10 to '
                        'the 6 candela per square meter, divided into scotopic, mesopic '
                        'and photopic vision, with starlight, moonlight, display black, '
                        'display white and an office desk, an overcast sky and sunlit snow.',
                        body)


def viewing_geometry():
    width, height = 720, 300
    body = [arrow_defs()]
    # Eye
    body.append(f'<circle cx="90" cy="150" r="42" fill="#ffffff" stroke="{INK}" '
                'stroke-width="2.5"/>')
    body.append(f'<circle cx="122" cy="150" r="13" fill="{INK}"/>')
    body.append(svg_text(90, 222, 'Eye', 15, INK, 700, 'middle'))
    # Screen
    body.append(f'<rect x="600" y="40" width="16" height="220" fill="#e6ecf0" '
                f'stroke="{INK}" stroke-width="2"/>')
    for index in range(10):
        y = 120 + index * 6
        fill = INK if index % 2 == 0 else '#ffffff'
        body.append(f'<rect x="600" y="{y}" width="16" height="6" fill="{fill}"/>')
    body.append(svg_text(640, 125, 'pixels of', 14, INK, 400))
    body.append(svg_text(640, 143, 'pitch p', 14, INK, 700))
    body.append(svg_text(608, 285, 'Screen', 15, INK, 700, 'middle'))
    # Rays forming one degree
    body.append(f'<path d="M 122 150 L 600 120" stroke="{L_COLOR}" stroke-width="2"/>')
    body.append(f'<path d="M 122 150 L 600 180" stroke="{L_COLOR}" stroke-width="2"/>')
    body.append(f'<path d="M 180 146 A 60 60 0 0 1 180 154" stroke="{L_COLOR}" '
                'fill="none" stroke-width="2"/>')
    body.append(svg_text(200, 140, '1°', 16, L_COLOR, 700))
    body.append(f'<path d="M 122 245 L 598 245" stroke="{MUTED}" stroke-width="1.6" '
                'marker-end="url(#arrow)"/>')
    body.append(svg_text(360, 238, 'viewing distance d', 15, INK, 400, 'middle'))
    body.append(svg_text(360, 40, 'pixels per degree = d × tan(1°) / p', 17, INK, 700,
                         'middle'))
    body.append(svg_text(360, 64, 'finest pattern (one-pixel stripes) = pixels per degree / 2 '
                         'cycles per degree', 14, MUTED, 400, 'middle'))
    return svg_document(width, height, 'Viewing geometry',
                        'An eye at distance d from a screen. One degree of visual angle '
                        'covers d times tan 1 degree on the screen, which divided by the '
                        'pixel pitch gives pixels per degree.', body)


def blind_spot():
    width, height = 720, 200
    body = [f'<path d="M 140 70 L 140 130 M 110 100 L 170 100" stroke="{INK}" '
            'stroke-width="6" stroke-linecap="round"/>',
            f'<circle cx="560" cy="100" r="26" fill="{L_COLOR}"/>',
            svg_text(140, 168, 'look here with the right eye', 14, MUTED, 400, 'middle'),
            svg_text(560, 168, 'this dot vanishes', 14, MUTED, 400, 'middle')]
    return svg_document(width, height, 'Blind spot test',
                        'A cross on the left and a red dot on the right. With the left '
                        'eye closed, fixating the cross with the right eye at the right '
                        'distance makes the dot fall on the blind spot and disappear.',
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
    'blind-spot.svg': blind_spot,
    'cone-sensitivity.svg': cone_sensitivity,
    'luminosity-functions.svg': luminosity_functions,
    'receptor-density.svg': receptor_density,
    'dark-adaptation.svg': dark_adaptation,
    'contrast-sensitivity.svg': contrast_sensitivity,
    'campbell-robson.svg': campbell_robson,
    'weber-demo.svg': weber_demo,
    'dither-integration.svg': dither_integration,
    'luma-chroma.svg': luma_chroma_demo,
    'chromatic-adaptation.svg': chromatic_adaptation,
    'opponent-channels.svg': opponent_channels,
    'light-levels.svg': light_levels,
    'viewing-geometry.svg': viewing_geometry,
    'tile-dithering.svg': tile_dithering,
    'palette-direct.svg': palette_direct,
}

EXTERNAL = {
    'eye-anatomy.jpg': {
        'title': '1413 Structure of the Eye',
        'author': 'OpenStax College',
        'source': 'https://commons.wikimedia.org/wiki/File:1413_Structure_of_the_Eye.jpg',
        'credit': 'Anatomy & Physiology, Connexions Web site, '
                  'https://cnx.org/content/col11496/1.6/, Jun 19, 2013',
        'license': 'CC BY 3.0',
        'license_url': 'https://creativecommons.org/licenses/by/3.0/',
        'changes': 'scaled to 1200 pixels wide and re-encoded as JPEG',
    },
    'retina-layers.jpg': {
        'title': '1414 Rods and Cones',
        'author': 'OpenStax College',
        'source': 'https://commons.wikimedia.org/wiki/File:1414_Rods_and_Cones.jpg',
        'credit': 'Anatomy & Physiology, Connexions Web site, '
                  'https://cnx.org/content/col11496/1.6/, Jun 19, 2013',
        'license': 'CC BY 3.0',
        'license_url': 'https://creativecommons.org/licenses/by/3.0/',
        'changes': 'scaled to 700 pixels wide and re-encoded as JPEG',
    },
    'retina-mosaic.jpg': {
        'title': 'Rods and Cones in Retina',
        'author': 'Johnny Tam, National Eye Institute (NIH)',
        'source': 'https://commons.wikimedia.org/wiki/File:Rods_and_Cones_in_Retina_(51175951237).jpg',
        'credit': 'NIH Image Gallery, https://www.flickr.com/photos/nihgov/51175951237/',
        'license': 'Public domain',
        'license_url': 'https://www.usa.gov/government-copyright',
        'changes': 'none',
    },
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {name: draw() for name, draw in FIGURES.items()}
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_visual_perception_figures.py',
        'plot_dependency': 'matplotlib 3.10, numpy, Pillow',
        'generated': sorted(FIGURES),
        'data_sources': {
            'cone-sensitivity.svg': 'Govardovskii et al. (2000) A1 alpha-band template, '
                                    'peaks 420, 498, 530 and 559 nm',
            'luminosity-functions.svg': 'CIE 1924 V(lambda) and CIE 1951 V\'(lambda), 10 nm',
            'receptor-density.svg': 'schematic curves shaped after Curcio et al. (1990)',
            'dark-adaptation.svg': 'schematic two-branch threshold curve',
            'contrast-sensitivity.svg': 'Mannos and Sakrison (1974) luminance model; '
                                        'schematic chroma low-pass after Mullen (1985)',
            'light-levels.svg': 'approximate luminances; regime limits from CIE 191:2010',
            'luma-chroma.svg': 'images/snake.png, BT.601 YCbCr on encoded values',
            'dither-integration.svg': 'Floyd-Steinberg and seeded random threshold dither',
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
        'external': EXTERNAL,
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
    for name in EXTERNAL:
        if not (OUT / name).exists():
            raise SystemExit(f'Missing external figure: {OUT / name}')
    print(f'{"Verified" if args.check else "Generated"} {len(assets)} assets')


if __name__ == '__main__':
    main()
