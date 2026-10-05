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


def contrast_sensitivity():
    f = np.logspace(-1, np.log10(60), 400)
    lum = 2.6 * (0.0192 + 0.114 * f) * np.exp(-(0.114 * f) ** 1.1)
    lum = lum / lum.max()
    chroma = 1.0 / (1.0 + (f / 1.5) ** 2.2)
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
        },
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
