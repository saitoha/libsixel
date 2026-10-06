#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the display technology concept figures.

Use Matplotlib 3.10, NumPy and Pillow.  Run without arguments to regenerate
the figures under docs/concepts/displays/figures, or with --check to verify
that the committed files are current.  Pixel densities are computed from
the listed resolutions and diagonals; viewing distances and the optical
values of the contrast figure are stated assumptions, not measurements.
"""

from __future__ import annotations

import argparse
import html
import io
import json
import math
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/concepts/displays/figures'
INK = '#183247'
MUTED = '#4c6070'
GRID = '#d9e1e7'
ACCENT = '#c0392b'
NS = 'http://www.w3.org/2000/svg'
RC = {'svg.fonttype': 'none', 'svg.hashsalt': 'libsixel-display-concepts',
      'font.family': 'sans-serif', 'font.sans-serif': ['DejaVu Sans', 'Arial'],
      'font.size': 11, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
      'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.titlesize': 12,
      'axes.titlecolor': INK, 'mathtext.default': 'regular'}
FAMILIES = {
    'print': ('Print and film', '#8e5a2b'),
    'crt': ('Cathode-ray tube', '#7a4fa0'),
    'lcd': ('Liquid crystal', '#17618c'),
    'emissive': ('Plasma, OLED, LED', '#c0392b'),
    'projection': ('Projection', '#2f8f5b'),
    'epaper': ('Electronic paper', '#5f6b73'),
    'density': ('Pixel density', '#b07d12'),
    'dec': ('DEC terminals and printers', '#0f7c80'),
}
# Families used only by the pixel-density figure.
DENSITY_FAMILIES = {
    'dec': ('DEC terminal', '#0f7c80'),
    'decprint': ('DEC printer', '#8e5a2b'),
    'crt': ('CRT monitor', '#7a4fa0'),
    'lcd': ('LCD monitor or notebook', '#17618c'),
    'density': ('High-density monitor or notebook', '#b07d12'),
    'emissive': ('Phone', '#c0392b'),
    'epaper': ('Electronic paper', '#5f6b73'),
    'tv': ('Television', '#2f8f5b'),
    'vr': ('VR headset', '#d35400'),
}


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


def hex_color(rgb):
    return '#%02x%02x%02x' % tuple(int(round(v)) for v in rgb)


# --------------------------------------------------------------------------
# Timeline

EVENTS = (
    (1881, 'print', 'Halftone printing', 'tones made of dots of varying size'),
    (1897, 'crt', 'Braun tube', 'the first cathode-ray tube display'),
    (1907, 'print', 'Autochrome', 'additive color from a mosaic of dyed starch grains'),
    (1935, 'print', 'Kodachrome', 'subtractive color slide film'),
    (1954, 'crt', 'RCA CT-100', 'mass-produced shadow-mask color television'),
    (1964, 'emissive', 'Plasma panel', 'built for the PLATO system'),
    (1968, 'lcd', 'Liquid crystal display', 'dynamic scattering, RCA'),
    (1968, 'crt', 'Sony Trinitron', 'aperture grille and phosphor stripes'),
    (1970, 'dec', 'DECwriter LA30', 'dot-matrix printing terminal, 5 × 7 glyphs'),
    (1971, 'lcd', 'Twisted nematic LCD', 'the basis of practical LCDs'),
    (1974, 'epaper', 'Gyricon', 'rotating two-color beads, Xerox PARC'),
    (1974, 'dec', 'DECwriter II LA36', '7 × 7 dot characters, 10 per inch'),
    (1978, 'dec', 'VT100', 'video text terminal'),
    (1983, 'dec', 'LA50', 'dot-matrix printer for SIXEL graphics, 144 × 72 dpi'),
    (1984, 'dec', 'VT240, VT241', '800 × 240 graphics; VT241: 4 of 64 colors'),
    (1986, 'dec', 'LA75', 'companion dot-matrix printer, 144 × 72 dpi'),
    (1987, 'dec', 'VT340', '800 × 480, 16 of 4,096 colors'),
    (1987, 'dec', 'LJ250', 'color inkjet SIXEL printer, 180 dpi'),
    (1987, 'emissive', 'Organic LED', 'Tang and VanSlyke, Kodak'),
    (1987, 'projection', 'Digital micromirror device', 'Hornbeck, Texas Instruments'),
    (1989, 'projection', 'Epson 3LCD projector', 'three LCD panels, one per primary'),
    (1996, 'lcd', 'IPS LCD', 'wide viewing angles, Hitachi'),
    (1997, 'emissive', '42-inch plasma TV', 'Fujitsu, 852 × 480'),
    (1997, 'projection', 'First DLP projector', 'color wheel and one mirror chip'),
    (1998, 'epaper', 'Electrophoretic ink', 'microcapsules of charged pigment, E Ink'),
    (2000, 'emissive', 'MicroLED', 'Jiang and Lin, Kansas State University'),
    (2001, 'density', 'IBM T220/T221', '3840 × 2400 at 204 ppi'),
    (2004, 'lcd', 'LCD monitors outsell CRTs', 'fixed native resolution'),
    (2004, 'epaper', 'Sony Librie', 'first E Ink e-reader'),
    (2007, 'epaper', 'Amazon Kindle', 'four gray levels'),
    (2007, 'emissive', 'Sony XEL-1', 'first OLED television'),
    (2009, 'density', '16:9 notebook panels', '1366 × 768 replaces taller screens'),
    (2010, 'density', 'iPhone 4 "Retina"', '326 ppi'),
    (2010, 'crt', 'SED abandoned', 'flat panel of tiny electron emitters'),
    (2012, 'density', 'Retina MacBook Pro', '2880 × 1800 at 220 ppi'),
    (2019, 'lcd', 'Mini LED television', 'LCD with about 1,000 dimming zones, TCL'),
    (2022, 'epaper', 'Kaleido 3, Gallery 3', 'color filters; four-pigment ink'),
    (2023, 'epaper', 'Spectra 6', 'six-color pigment signage'),
    (2025, 'lcd', 'RGB Mini LED', 'red, green and blue LEDs in each backlight zone'),
)


def timeline():
    width, row = 800, 27
    height = 124 + row * len(EVENTS)
    body = [svg_text(width / 2, 30, 'Display technologies, 1881–2025', 17, INK, 700, 'middle')]
    for index, (key, (label, color)) in enumerate(FAMILIES.items()):
        x = 20 + (index % 4) * 190
        y = 56 + (index // 4) * 22
        body.append(f'<circle cx="{x + 6}" cy="{y}" r="6" fill="{color}"/>')
        body.append(svg_text(x + 17, y + 5, label, 13, MUTED))
    top = 112
    body.append(f'<line x1="86" y1="{top - 10}" x2="86" y2="{height - 10}" stroke="{GRID}" '
                'stroke-width="2"/>')
    previous = None
    for index, (year, family, title, detail) in enumerate(EVENTS):
        y = top + index * row
        if year != previous:
            body.append(svg_text(70, y + 5, str(year), 14, INK, 700, 'end'))
        previous = year
        body.append(f'<circle cx="86" cy="{y}" r="6" fill="{FAMILIES[family][1]}"/>')
        body.append(f'<text x="104" y="{y + 5}" font-size="14" fill="{INK}">'
                    f'<tspan font-weight="700">{html.escape(title)}</tspan>'
                    f'<tspan fill="{MUTED}"> — {html.escape(detail)}</tspan></text>')
    return svg_document(width, height, 'Display technology timeline',
                        'A timeline from halftone printing in 1881 to RGB Mini LED '
                        'backlights in 2025, with each event colored by technology family.',
                        body)


# --------------------------------------------------------------------------
# Light paths and ambient light

def light_paths():
    width, height = 780, 270
    body = []

    def arrow(x1, y1, x2, y2, color, dash=False):
        angle = math.atan2(y2 - y1, x2 - x1)
        head = [(x2, y2),
                (x2 - 10 * math.cos(angle - 0.4), y2 - 10 * math.sin(angle - 0.4)),
                (x2 - 10 * math.cos(angle + 0.4), y2 - 10 * math.sin(angle + 0.4))]
        style = ' stroke-dasharray="6 4"' if dash else ''
        body.append(f'<line x1="{x1:g}" y1="{y1:g}" x2="{x2:g}" y2="{y2:g}" stroke="{color}" '
                    f'stroke-width="2.5"{style}/>')
        body.append('<polygon points="' + ' '.join(f'{px:.1f},{py:.1f}' for px, py in head)
                    + f'" fill="{color}"/>')

    def eye(x, y):
        body.append(f'<ellipse cx="{x}" cy="{y}" rx="18" ry="11" fill="#ffffff" stroke="{INK}" '
                    'stroke-width="2"/>')
        body.append(f'<circle cx="{x - 4}" cy="{y}" r="6" fill="{INK}"/>')

    columns = (
        ('Emissive', 'CRT, plasma, OLED, LED', 'The pixel itself emits light.'),
        ('Transmissive', 'LCD, slide film, LCD projector', 'A backlight shines through a filter.'),
        ('Reflective', 'Print, electronic paper', 'Room light bounces off the surface.'),
    )
    for index, (title, examples, note) in enumerate(columns):
        x0 = 20 + index * 260
        body.append(svg_text(x0 + 110, 28, title, 16, INK, 700, 'middle'))
        body.append(svg_text(x0 + 110, 48, examples, 12, MUTED, 400, 'middle'))
        body.append(f'<rect x="{x0 + 40}" y="80" width="22" height="120" fill="#e9eef2" '
                    f'stroke="{MUTED}"/>')
        eye(x0 + 200, 140)
        if index == 0:
            body.append(f'<circle cx="{x0 + 51}" cy="140" r="9" fill="#f2c94c"/>')
            arrow(x0 + 64, 140, x0 + 176, 140, '#d39b00')
        elif index == 1:
            body.append(f'<rect x="{x0 + 2}" y="80" width="14" height="120" fill="#f2c94c"/>')
            body.append(svg_text(x0 + 9, 216, 'backlight', 11, MUTED, 400, 'middle'))
            arrow(x0 + 18, 140, x0 + 38, 140, '#d39b00')
            arrow(x0 + 64, 140, x0 + 176, 140, '#b5651d')
        else:
            body.append(f'<circle cx="{x0 + 160}" cy="80" r="12" fill="#f2c94c"/>')
            body.append(svg_text(x0 + 160, 112, 'room light', 11, MUTED, 400, 'middle'))
            arrow(x0 + 148, 88, x0 + 66, 128, '#d39b00', True)
            arrow(x0 + 66, 150, x0 + 176, 146, '#b5651d')
        body.append(svg_text(x0 + 110, 240, note, 13, INK, 400, 'middle'))
    return svg_document(width, height, 'Emissive, transmissive and reflective displays',
                        'Three diagrams. An emissive pixel sends its own light to the eye. '
                        'A transmissive display filters light from a backlight. A reflective '
                        'surface returns part of the room light.', body)


AMBIENT = {'emissive_white_cd_m2': 300, 'lcd_native_contrast': 1000,
           'oled_black_cd_m2': 0.0005, 'screen_reflectance': 0.02,
           'epaper_white_reflectance': 0.40, 'epaper_black_reflectance': 0.03,
           'print_white_reflectance': 0.85, 'print_black_reflectance': 0.03}


def ambient():
    lux = np.logspace(0, 5, 200)
    a = AMBIENT
    reflected = a['screen_reflectance'] * lux / math.pi
    white = a['emissive_white_cd_m2']
    lcd = (white + reflected) / (white / a['lcd_native_contrast'] + reflected)
    oled = (white + reflected) / (a['oled_black_cd_m2'] + reflected)
    epaper = np.full_like(lux, a['epaper_white_reflectance'] / a['epaper_black_reflectance'])
    paper = np.full_like(lux, a['print_white_reflectance'] / a['print_black_reflectance'])
    with plt.rc_context(RC):
        fig, (left, right) = plt.subplots(1, 2, figsize=(8.2, 3.4), gridspec_kw={'wspace': 0.32})
        left.loglog(lux, np.full_like(lux, white), color=FAMILIES['emissive'][1], lw=2,
                    label='emissive or backlit display')
        left.loglog(lux, a['epaper_white_reflectance'] * lux / math.pi,
                    color=FAMILIES['epaper'][1], lw=2, label='electronic paper')
        left.loglog(lux, a['print_white_reflectance'] * lux / math.pi,
                    color=FAMILIES['print'][1], lw=2, ls='--', label='printed paper')
        left.set_title('Luminance of white')
        left.set_ylabel('cd/m²')
        right.loglog(lux, oled, color=FAMILIES['emissive'][1], lw=2, label='OLED')
        right.loglog(lux, lcd, color=FAMILIES['lcd'][1], lw=2, label='LCD')
        right.loglog(lux, paper, color=FAMILIES['print'][1], lw=2, ls='--', label='printed paper')
        right.loglog(lux, epaper, color=FAMILIES['epaper'][1], lw=2, label='electronic paper')
        right.set_title('Contrast ratio')
        for ax in (left, right):
            ax.set_xlabel('room illuminance (lux)')
            ax.grid(True, which='major', color=GRID, lw=0.8)
            for value in (300, 10000, 100000):
                ax.axvline(value, color=GRID, lw=1.2, zorder=0)
            ax.legend(frameon=False, fontsize=9, loc='lower left' if ax is right else 'lower right')
        left.set_ylim(0.1, 1e5)
        right.set_ylim(1, 1e6)
        for ax, top in ((left, 5e4), (right, 5e5)):
            for value, name in ((300, 'office'), (10000, 'daylight'), (100000, 'sun')):
                ax.text(value, top, name, ha='right', va='center', fontsize=8.5, color=MUTED,
                        rotation=90)
        return mpl_svg(fig)


# --------------------------------------------------------------------------
# Color mixing

def disks(size, centers, radius):
    yy, xx = np.mgrid[0:size, 0:size] + 0.5
    return [((xx - cx) ** 2 + (yy - cy) ** 2) <= radius ** 2 for cx, cy in centers]


def mixing():
    size = 200
    centers = [(100, 72), (72, 122), (128, 122)]
    masks = disks(size, centers, 52)
    subtractive = np.ones((size, size, 3))
    for mask, ink in zip(masks, ((0, 1, 1), (1, 0, 1), (1, 1, 0))):
        subtractive[mask] *= ink
    additive = np.zeros((size, size, 3))
    for mask, light in zip(masks, ((1, 0, 0), (0, 1, 0), (0, 0, 1))):
        additive[mask] += light
    additive = np.clip(additive, 0, 1)
    stripes = np.zeros((size, size, 3))
    cell = 20
    for x in range(0, size, cell):
        for channel in range(3):
            lit = channel < 2
            x0 = x + channel * cell // 3
            stripes[4:size - 4, x0 + 1:x0 + cell // 3 - 1, channel] = 1 if lit else 0.08
    fields = np.zeros((size, size, 3))
    for index in range(3):
        fields[:, index * size // 3:(index + 1) * size // 3, index] = 1
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 4, figsize=(8.4, 2.9), gridspec_kw={'wspace': 0.12})
        panels = (
            (subtractive, 'Subtractive', 'inks absorb light:\nprint, slide film'),
            (additive, 'Additive', 'lights add up:\noverlapping projectors'),
            (stripes, 'Spatial additive', 'red and green subpixels\nlook yellow from afar'),
            (fields, 'Temporal additive', 'red, green, blue in turn\nlook white: DLP'),
        )
        for ax, (image, title, note) in zip(axes, panels):
            ax.imshow(image, interpolation='none')
            ax.set_title(title, fontweight='bold')
            ax.set_xlabel(note, fontsize=10, color=MUTED)
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_color(GRID)
        axes[3].text(100, 196, 'time →', ha='center', va='bottom', color='white', fontsize=10)
        return mpl_svg(fig)


# --------------------------------------------------------------------------
# Subpixel layouts

RGB = {'R': '#e0453a', 'G': '#3cb44b', 'B': '#3b6fd8'}


def subpixels():
    width, height = 800, 250
    tile = 120
    body = []
    titles = (('Shadow mask', 'CRT phosphor triads'), ('Aperture grille', 'CRT stripes'),
              ('RGB stripe', 'LCD'), ('RGBG', 'OLED, shared subpixels'),
              ('Color filter', 'over gray electronic ink'))
    for index, (title, note) in enumerate(titles):
        x0 = 20 + index * 156
        y0 = 50
        body.append(svg_text(x0 + tile / 2, 24, title, 14, INK, 700, 'middle'))
        body.append(svg_text(x0 + tile / 2, 42, note, 11, MUTED, 400, 'middle'))
        body.append(f'<rect x="{x0}" y="{y0}" width="{tile}" height="{tile}" fill="#111820"/>')
        if index == 0:
            for row in range(6):
                for col in range(7):
                    cx = x0 + 10 + col * 17 + (8.5 if row % 2 else 0)
                    cy = y0 + 11 + row * 19.5
                    color = 'RGB'[(col * 2 + row) % 3]
                    if cx < x0 + tile - 4:
                        body.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="6.5" '
                                    f'fill="{RGB[color]}"/>')
        elif index == 1:
            for col in range(12):
                body.append(f'<rect x="{x0 + 2 + col * 10}" y="{y0}" width="7" height="{tile}" '
                            f'fill="{RGB["RGB"[col % 3]]}"/>')
            body.append(f'<line x1="{x0}" y1="{y0 + 40}" x2="{x0 + tile}" y2="{y0 + 40}" '
                        'stroke="#111820" stroke-width="1.5"/>')
        elif index == 2:
            for row in range(4):
                for col in range(12):
                    body.append(f'<rect x="{x0 + 1.5 + col * 10}" y="{y0 + 2 + row * 30}" '
                                f'width="7" height="26" fill="{RGB["RGB"[col % 3]]}"/>')
        elif index == 3:
            for row in range(6):
                order = 'RGBG' if row % 2 == 0 else 'BGRG'
                for col in range(8):
                    color = order[col % 4]
                    size = 9 if color == 'G' else 13
                    cx = x0 + 8 + col * 15
                    cy = y0 + 10 + row * 20
                    body.append(f'<rect x="{cx - size / 2:.1f}" y="{cy - size / 2:.1f}" '
                                f'width="{size}" height="{size}" rx="2" fill="{RGB[color]}"/>')
        else:
            gray = (0.9, 0.55, 0.9, 0.25, 0.9, 0.7)
            for row in range(4):
                for col in range(4):
                    level = gray[(row * 4 + col) % len(gray)]
                    base = np.array([level * 235 + 10] * 3)
                    tint = np.array([[255, 150, 150], [150, 230, 150],
                                     [150, 170, 255]][(row + col) % 3]) / 255
                    fill = hex_color(base * (0.55 + 0.45 * tint))
                    body.append(f'<rect x="{x0 + col * 30}" y="{y0 + row * 30}" width="29" '
                                f'height="29" fill="{fill}"/>')
        body.append(f'<rect x="{x0}" y="{y0}" width="{tile}" height="{tile}" fill="none" '
                    f'stroke="{MUTED}"/>')
    body.append(svg_text(width / 2, 210, 'Each display builds color from small red, green and '
                         'blue elements; the eye averages them when they are fine enough.', 13,
                         MUTED, 400, 'middle'))
    body.append(svg_text(width / 2, 232, 'Schematic patterns, not to scale.', 12, MUTED, 400,
                         'middle'))
    return svg_document(width, height, 'Subpixel layouts',
                        'Five schematic tiles: CRT shadow-mask phosphor triads, CRT '
                        'aperture-grille stripes, LCD RGB stripes, OLED RGBG layout with shared '
                        'subpixels, and a color filter array over gray electronic ink.', body)


# --------------------------------------------------------------------------
# Pixel density

TAN1 = math.tan(math.radians(1))


def ppd_from_ppi(ppi, distance_cm):
    return distance_cm / 2.54 * ppi * TAN1


# Each entry: (year, family, label, kind, parameters, distance cm).
#   panel: (diagonal in, width px, height px)
#   aspect: (diagonal in, width px, height px, screen aspect) for non-square
#           pixels; the range spans horizontal and vertical density
#   crt: (viewable diagonal in, width px, height px, phosphor pitch mm); the
#        range spans the addressable density and the phosphor-pitch limit
#   dpi: (horizontal dpi, vertical dpi); a range when they differ
#   ppd: (published pixels per degree,)
DISPLAYS = (
    (1983, 'decprint', 'LA50', 'dpi', (144, 72), 35),
    (1984, 'dec', 'VT240', 'aspect', (11.0, 800, 240, 4 / 3), 60),
    (1986, 'decprint', 'LA75', 'dpi', (144, 72), 35),
    (1987, 'dec', 'VT340', 'aspect', (12.0, 800, 480, 4 / 3), 60),
    (1987, 'decprint', 'LJ250', 'dpi', (180, 180), 35),
    (1987, 'crt', '14" VGA', 'panel', (13.0, 640, 480), 60),
    (1995, 'crt', '17" CRT', 'panel', (16.0, 1024, 768), 60),
    (1999, 'crt', '21" CRT 1600 × 1200', 'panel', (20.0, 1600, 1200), 60),
    (2002, 'crt', 'Sony FW900', 'crt', (22.5, 2304, 1440, 0.25), 60),
    (2003, 'crt', '22" CRT 2048 × 1536', 'crt', (20.0, 2048, 1536, 0.24), 60),
    (2001, 'lcd', '15" LCD', 'panel', (15.0, 1024, 768), 60),
    (2003, 'lcd', '17" LCD', 'panel', (17.0, 1280, 1024), 60),
    (2005, 'lcd', '19" LCD', 'panel', (19.0, 1280, 1024), 60),
    (2009, 'lcd', '21.5" 1080p', 'panel', (21.5, 1920, 1080), 60),
    (2006, 'lcd', '15" UXGA notebook', 'panel', (15.0, 1600, 1200), 50),
    (2009, 'lcd', '15.6" 1366 × 768 notebook', 'panel', (15.6, 1366, 768), 50),
    (2001, 'density', 'IBM T221', 'panel', (22.2, 3840, 2400), 60),
    (2012, 'density', 'Retina MacBook Pro', 'panel', (15.4, 2880, 1800), 50),
    (2014, 'density', '27" 5K', 'panel', (27.0, 5120, 2880), 60),
    (2018, 'density', '27" 4K', 'panel', (27.0, 3840, 2160), 60),
    (2019, 'density', 'Pro Display XDR', 'panel', (32.0, 6016, 3384), 60),
    (2021, 'density', 'MacBook Pro 14"', 'panel', (14.2, 3024, 1964), 50),
    (2007, 'emissive', 'iPhone', 'panel', (3.5, 320, 480), 30),
    (2010, 'emissive', 'iPhone 4', 'panel', (3.5, 640, 960), 30),
    (2017, 'emissive', 'iPhone X', 'panel', (5.8, 1125, 2436), 30),
    (2019, 'emissive', 'Xperia 1', 'panel', (6.5, 1644, 3840), 30),
    (2023, 'emissive', 'iPhone 15 Pro', 'panel', (6.1, 1179, 2556), 30),
    (2007, 'epaper', 'Kindle', 'panel', (6.0, 600, 800), 35),
    (2015, 'epaper', 'Kindle Paperwhite', 'panel', (6.0, 1072, 1448), 35),
    (2021, 'epaper', 'Paperwhite 6.8"', 'panel', (6.8, 1236, 1648), 35),
    (2024, 'epaper', 'Kobo Libra Colour', 'dpi', (300, 150), 35),
    (2018, 'tv', '65" 4K TV at 2.5 m', 'panel', (65.0, 3840, 2160), 250),
    (2023, 'vr', 'Meta Quest 3', 'ppd', (25,), None),
)


def density_rows():
    rows = []
    for year, family, label, kind, values, distance in DISPLAYS:
        row = {'year': year, 'family': family, 'label': label, 'kind': kind,
               'distance_cm': distance}
        if kind == 'panel':
            diagonal, w, h = values
            ppi = math.hypot(w, h) / diagonal
            row.update(diagonal_in=diagonal, pixels=[w, h], ppi=round(ppi, 1))
            high = low = ppd_from_ppi(ppi, distance)
        elif kind == 'aspect':
            diagonal, w, h, aspect = values
            height = diagonal / math.hypot(aspect, 1)
            width = height * aspect
            row.update(viewable_diagonal_in=diagonal, pixels=[w, h],
                       ppi=[round(w / width, 1), round(h / height, 1)])
            high, low = (ppd_from_ppi(w / width, distance), ppd_from_ppi(h / height, distance))
        elif kind == 'crt':
            diagonal, w, h, pitch = values
            ppi = math.hypot(w, h) / diagonal
            row.update(viewable_diagonal_in=diagonal, pixels=[w, h], ppi=round(ppi, 1),
                       phosphor_pitch_mm=pitch)
            high = ppd_from_ppi(ppi, distance)
            low = min(high, distance * 10 * TAN1 / pitch)
        elif kind == 'dpi':
            row.update(dpi=list(values))
            high, low = (ppd_from_ppi(values[0], distance), ppd_from_ppi(values[1], distance))
        else:
            high = low = values[0]
            row.update(source='published by the manufacturer')
        row.update(ppd=round(high, 1), ppd_low=round(low, 1), cpd=round(high / 2, 1))
        rows.append(row)
    return rows


# Offsets in points for numbers that would otherwise collide.
NUMBER_OFFSETS = {'LA50': (-6, 0), 'VT240': (-6, 4), 'LA75': (6, -6), 'VT340': (6, 6),
                  '14" VGA': (6, -2), '22" CRT 2048 × 1536': (6, 4),
                  'Sony FW900': (-11, 2), '15" UXGA notebook': (-8, 7), 'Kindle': (-12, 2),
                  '15.6" 1366 × 768 notebook': (6, -5), '21.5" 1080p': (6, 4),
                  'iPhone 15 Pro': (6, 4), 'MacBook Pro 14"': (6, -6),
                  'Pro Display XDR': (-11, 5), '27" 5K': (-11, 3)}


def density():
    rows = sorted(density_rows(), key=lambda r: (r['year'], r['label']))
    with plt.rc_context(RC):
        fig = plt.figure(figsize=(9.6, 8.6))
        grid = fig.add_gridspec(2, 1, height_ratios=(2.2, 1), hspace=0.32)
        ax = fig.add_subplot(grid[0])
        key = fig.add_subplot(grid[1])
        key.axis('off')
        ax.axhspan(60, 140, color='#eef4ea', zorder=0)
        ax.axhspan(100, 120, color='#dcebd5', zorder=0)
        ax.axhline(60, color='#5a8f4a', lw=1.2, ls='--')
        ax.text(1981, 61.5, '60 pixels/degree ≈ 30 cpd: 20/20 acuity', color='#3d6b31',
                fontsize=9, va='bottom')
        ax.text(1981, 101.5, 'resolution limit near 50–60 cpd', color='#3d6b31', fontsize=9,
                va='bottom')
        lines = []
        for number, row in enumerate(rows, 1):
            color = DENSITY_FAMILIES[row['family']][1]
            hollow = row['family'] == 'crt'
            x, high, low = row['year'], row['ppd'], row['ppd_low']
            if low < high:
                ax.plot([x, x], [low, high], color=color, lw=3, alpha=0.5,
                        solid_capstyle='round', zorder=2)
            ax.scatter(x, high, s=40, zorder=3, facecolor='white' if hollow else color,
                       edgecolor=color, lw=1.8)
            dx, dy = NUMBER_OFFSETS.get(row['label'], (6, 0))
            ax.annotate(str(number), (x, high), xytext=(dx, dy), textcoords='offset points',
                        fontsize=8, color=INK, fontweight='bold', va='center',
                        ha='left' if dx > 0 else 'right')
            lines.append((number, row, color))
        for a, b in (('15" UXGA notebook', '15.6" 1366 × 768 notebook'),
                     ('17" LCD', '19" LCD')):
            ra = next(r for r in rows if r['label'] == a)
            rb = next(r for r in rows if r['label'] == b)
            ax.annotate('', xy=(rb['year'], rb['ppd']), xytext=(ra['year'], ra['ppd']),
                        arrowprops={'arrowstyle': '->', 'color': ACCENT, 'lw': 1.6})
        ax.set_xlim(1980, 2027)
        ax.set_ylim(0, 140)
        ax.set_xlabel('year')
        ax.set_ylabel('pixels per degree at a typical distance')
        secondary = ax.secondary_yaxis('right', functions=(lambda v: v / 2, lambda v: v * 2))
        secondary.set_ylabel('finest pattern (cycles per degree)')
        ax.grid(True, color=GRID, lw=0.8)
        ax.set_title('Pixels per degree of visual angle, 1983–2024', fontweight='bold')
        handles = [plt.Line2D([], [], marker='o', ls='', color=color,
                              markerfacecolor='white' if family == 'crt' else color, label=label)
                   for family, (label, color) in DENSITY_FAMILIES.items()]
        ax.legend(handles=handles, frameon=False, fontsize=8.5, ncol=3, loc='upper left',
                  bbox_to_anchor=(0.0, 0.97))
        rows_per_column = math.ceil(len(lines) / 3)
        for index, (number, row, color) in enumerate(lines):
            column, line = divmod(index, rows_per_column)
            y = 1.0 - line / rows_per_column
            key.text(0.03 + column * 0.34, y, f'{number}', transform=key.transAxes, fontsize=8.5,
                     fontweight='bold', color=color, ha='right', va='top')
            key.text(0.045 + column * 0.34, y, f"{row['label']} ({row['year']})",
                     transform=key.transAxes, fontsize=8.5, color=INK, va='top')
        return mpl_svg(fig)


# --------------------------------------------------------------------------
# Electronic paper palettes

SIX_INKS = np.array([[0, 0, 0], [255, 255, 255], [255, 0, 0], [255, 255, 0],
                     [0, 0, 255], [0, 255, 0]])


def palette_image(colors):
    flat = np.zeros((256, 3), dtype=np.uint8)
    flat[:len(colors)] = colors
    flat[len(colors):] = colors[0]
    image = Image.new('P', (1, 1))
    image.putpalette(flat.flatten().tolist())
    return image


def epaper_palettes():
    source = Image.open(ROOT / 'images/snake.png').convert('RGB')
    source = source.resize((240, 180), Image.Resampling.LANCZOS)
    grays = np.array([[17 * i] * 3 for i in range(16)])
    adaptive = source.quantize(colors=16, method=Image.Quantize.MEDIANCUT)
    adaptive_colors = np.array(adaptive.getpalette()[:48]).reshape(16, 3)
    snapped = np.round(adaptive_colors / 17) * 17
    gray_source = source.convert('L').convert('RGB')
    panels = (
        (source, 'Original', 'full color'),
        (gray_source.quantize(palette=palette_image(grays),
                              dither=Image.Dither.FLOYDSTEINBERG),
         '16 grays', 'monochrome e-reader'),
        (source.quantize(palette=palette_image(SIX_INKS), dither=Image.Dither.FLOYDSTEINBERG),
         '6 fixed inks', 'idealized six-color panel'),
        (source.quantize(palette=palette_image(snapped), dither=Image.Dither.FLOYDSTEINBERG),
         '16 of 4,096', 'adaptive, 4 bits per channel'),
    )
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 4, figsize=(8.4, 2.4), gridspec_kw={'wspace': 0.06})
        for ax, (image, title, note) in zip(axes, panels):
            ax.imshow(image.convert('RGB'), interpolation='none')
            ax.set_title(title, fontweight='bold')
            ax.set_xlabel(note, fontsize=10, color=MUTED)
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_color(GRID)
        return mpl_svg(fig), snapped.astype(int).tolist()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    epaper_svg, snapped = epaper_palettes()
    assets = {
        'timeline.svg': timeline(),
        'light-paths.svg': light_paths(),
        'ambient-light.svg': ambient(),
        'color-mixing.svg': mixing(),
        'subpixels.svg': subpixels(),
        'pixel-density.svg': density(),
        'epaper-palettes.svg': epaper_svg,
    }
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_display_concept_figures.py',
        'plot_dependency': 'matplotlib 3.10, numpy, Pillow',
        'timeline': [{'year': y, 'family': f, 'event': t, 'detail': d}
                     for y, f, t, d in EVENTS],
        'pixel_density': {'formula': 'ppd = distance * tan(1 degree) * ppi; cpd = ppd / 2',
                          'note': 'CRT entries use the addressable resolution and the '
                                  'viewable diagonal; where a phosphor pitch is given, '
                                  'ppd_low is the pitch-limited density. DEC terminal '
                                  'entries estimate the viewable diagonal as the nominal '
                                  'tube size minus one inch; their ppd and ppd_low are the '
                                  'horizontal and vertical densities of non-square pixels. '
                                  'Printer entries use horizontal and vertical dpi at 35 cm. '
                                  'Meta Quest 3 uses the manufacturer\'s published value.',
                          'displays': density_rows()},
        'ambient_light_assumptions': AMBIENT,
        'epaper_palettes': {'source': 'images/snake.png at 240 x 180',
                            'dither': 'Pillow Floyd-Steinberg',
                            'gray_levels': 16,
                            'six_inks_idealized_srgb': SIX_INKS.tolist(),
                            'adaptive_16_snapped_to_4_bits': snapped},
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
