#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the palette concept figures.

Use Matplotlib 3.10, NumPy and Pillow.  Run without arguments to regenerate
the figures under docs/concepts/palettes/figures, or with --check to verify
that the committed files are current.  The xterm 256-color and VT340 color
swatches are read from libsixel's own built-in tables in src/dither.c.
"""

from __future__ import annotations

import argparse
import html
import io
import json
import re
from pathlib import Path

import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/concepts/palettes/figures'
INK = '#183247'
MUTED = '#4c6070'
GRID = '#d9e1e7'
ACCENT = '#c0392b'
NS = 'http://www.w3.org/2000/svg'
RC = {'svg.fonttype': 'none', 'svg.hashsalt': 'libsixel-palette-concepts',
      'font.family': 'sans-serif', 'font.sans-serif': ['DejaVu Sans', 'Arial'],
      'font.size': 12, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
      'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.titlesize': 12,
      'axes.titlecolor': INK}


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
    return '#%02x%02x%02x' % tuple(int(v) for v in rgb)


def builtin_table(name):
    """Read a built-in palette table from src/dither.c."""
    source = (ROOT / 'src/dither.c').read_text()
    match = re.search(r'static const unsigned char ' + name + r'\[\] = \{(.*?)\};', source, re.S)
    body = re.sub(r'/\*.*?\*/', '', match.group(1), flags=re.S)
    values = []
    for item in body.split(','):
        item = item.strip()
        if not item:
            continue
        if not re.fullmatch(r'[0-9xa-fA-F*/+\- ()]+', item):
            raise SystemExit(f'unexpected table entry in {name}: {item!r}')
        values.append(int(eval(item.replace('/', '//'))))  # noqa: S307
    return np.array(values, dtype=int).reshape(-1, 3)


# --------------------------------------------------------------------------
# Figures


ART = ['00000000',
       '01111000',
       '01221100',
       '01233110',
       '01233110',
       '00111100']


def indexed_image():
    palettes = (('#1f2a44', '#e8b04b', '#f5f1e6', '#c0392b'),
                ('#1f2a44', '#3a7d44', '#f5f1e6', '#2f5fb3'))
    width, height = 760, 330
    cell = 30
    body = []

    def grid(x0, y0, colors=None):
        for y, row in enumerate(ART):
            for x, value in enumerate(row):
                fill = colors[int(value)] if colors else '#ffffff'
                body.append(f'<rect x="{x0 + x * cell}" y="{y0 + y * cell}" width="{cell}" '
                            f'height="{cell}" fill="{fill}" stroke="{MUTED}" stroke-width="0.6"/>')
                if not colors:
                    body.append(svg_text(x0 + x * cell + cell / 2, y0 + y * cell + 20, value, 13,
                                         INK, 700, 'middle'))

    def table(x0, y0, colors, highlight=None):
        for index, color in enumerate(colors):
            y = y0 + index * 34
            stroke = ACCENT if index in (highlight or ()) else MUTED
            width_ = 2.5 if index in (highlight or ()) else 1
            body.append(f'<rect x="{x0}" y="{y}" width="34" height="28" fill="#f4f7f9" '
                        f'stroke="{MUTED}"/>')
            body.append(svg_text(x0 + 17, y + 19, str(index), 14, INK, 700, 'middle'))
            body.append(f'<rect x="{x0 + 40}" y="{y}" width="64" height="28" fill="{color}" '
                        f'stroke="{stroke}" stroke-width="{width_}"/>')

    body.append(svg_text(150, 30, 'Pixels store indices', 16, INK, 700, 'middle'))
    grid(30, 50)
    body.append(svg_text(340, 30, 'Palette', 16, INK, 700, 'middle'))
    table(288, 60, palettes[0])
    body.append(svg_text(560, 30, 'What you see', 16, INK, 700, 'middle'))
    grid(440, 50, palettes[0])
    body.append(svg_text(width / 2, 250, 'Change palette entries 1 and 3, and every pixel '
                         'that uses them changes at once:', 14, MUTED, 400, 'middle'))
    x = 150
    for index, color in enumerate(palettes[1]):
        stroke = ACCENT if index in (1, 3) else MUTED
        body.append(f'<rect x="{x + index * 70}" y="268" width="56" height="26" fill="{color}" '
                    f'stroke="{stroke}" stroke-width="{2.5 if index in (1, 3) else 1}"/>')
        body.append(svg_text(x + index * 70 + 28, 314, str(index), 13, INK, 700, 'middle'))
    small = 10
    for y, row in enumerate(ART):
        for xx, value in enumerate(row):
            body.append(f'<rect x="{470 + xx * small}" y="{262 + y * small}" width="{small}" '
                        f'height="{small}" fill="{palettes[1][int(value)]}"/>')
    return svg_document(width, height, 'Indexed color',
                        'An eight by six image stores small numbers. A four-entry palette '
                        'maps each number to a color. Changing palette entries 1 and 3 '
                        'recolors every pixel that uses them without touching the pixels.',
                        body)


EVENTS = (
    (1973, 'SuperPaint frame buffer', '8-bit pixels through a color map'),
    (1981, 'IBM CGA', '4 fixed colors at 320 × 200'),
    (1984, 'IBM EGA', '16 of 64 colors'),
    (1985, 'Amiga', '32 of 4,096; HAM mode'),
    (1987, 'IBM VGA', '256 of 262,144 colors'),
    (1987, 'DEC VT340 (SIXEL)', '16 of 4,096 colors'),
    (1987, 'GIF87a', '256-color palettes'),
    (1989, 'GIF89a', 'animation, transparency'),
    (1995, 'Web-safe palette', '216 shared colors'),
    (1996, 'PNG', 'palettes with alpha per entry'),
    (1998, 'S3TC texture blocks', '4-color palette per 4×4 block'),
    (1999, 'xterm 256 colors', '16 + 216 + 24 grays'),
    (2015, 'PICO-8', 'a fixed 16-color palette'),
)


def timeline():
    width, row = 760, 32
    height = 70 + row * len(EVENTS)
    body = [svg_text(width / 2, 32, 'Palettes in hardware, formats, and terminals', 17, INK, 700,
                     'middle'),
            f'<line x1="96" y1="58" x2="96" y2="{height - 18}" stroke="{INK}" stroke-width="2"/>']
    previous = None
    for index, (year, title, detail) in enumerate(EVENTS):
        y = 70 + index * row
        if year != previous:
            body.append(svg_text(78, y + 5, str(year), 15, INK, 700, 'end'))
        previous = year
        body.append(f'<circle cx="96" cy="{y}" r="5.5" fill="{INK}"/>')
        body.append(f'<text x="116" y="{y + 5}" font-size="15" fill="{INK}">'
                    f'<tspan font-weight="700">{html.escape(title)}</tspan>'
                    f'<tspan fill="{MUTED}"> — {html.escape(detail)}</tspan></text>')
    return svg_document(width, height, 'Palette timeline',
                        'A timeline of palette-based displays, file formats, texture '
                        'compression, and terminals from 1973 to 2015.', body)


def palette_sizes():
    source = Image.open(ROOT / 'images/snake.png').convert('RGB')
    source = source.resize((240, 180), Image.Resampling.LANCZOS)
    with plt.rc_context(RC):
        fig, axes = plt.subplots(1, 4, figsize=(7.6, 1.9), gridspec_kw={'wspace': 0.05})
        for ax, count in zip(axes, (2, 4, 16, 256)):
            palette = source.quantize(colors=count, method=Image.Quantize.MEDIANCUT)
            image = source.quantize(palette=palette,
                                    dither=Image.Dither.FLOYDSTEINBERG).convert('RGB')
            ax.imshow(image, interpolation='none')
            ax.set_title(f'{count} colors')
            ax.set_xticks([])
            ax.set_yticks([])
            for spine in ax.spines.values():
                spine.set_color(GRID)
        return mpl_svg(fig)


def standard_palettes():
    xterm = builtin_table('pal_xterm256')
    vt340 = builtin_table('pal_vt340_color')
    levels = [0, 51, 102, 153, 204, 255]
    web = np.array([[r, g, b] for r in levels for g in levels for b in levels])
    width, height = 760, 380
    body = []

    def swatches(x0, y0, colors, columns, size, title, note):
        body.append(svg_text(x0, y0 - 28, title, 15, INK, 700))
        body.append(svg_text(x0, y0 - 10, note, 12, MUTED))
        rows = -(-len(colors) // columns)
        body.append(f'<rect x="{x0 - 1}" y="{y0 - 1}" width="{columns * size + 2:g}" '
                    f'height="{rows * size + 2:g}" fill="none" stroke="{GRID}"/>')
        for index, color in enumerate(colors):
            x = x0 + (index % columns) * size
            y = y0 + (index // columns) * size
            body.append(f'<rect x="{x}" y="{y}" width="{size}" height="{size}" '
                        f'fill="{hex_color(color)}"/>')

    swatches(20, 60, xterm[:16], 16, 31.5, 'xterm 256-color palette',
             '16 system colors, then the 6×6×6 cube and 24 grays')
    swatches(20, 100, xterm[16:232], 36, 14, '', '')
    swatches(20, 190, xterm[232:], 24, 21, '', '')
    swatches(20, 280, web, 36, 14, 'Web-safe palette',
             '6×6×6 levels of 0, 51, 102, 153, 204, 255')
    swatches(560, 60, vt340, 4, 44, 'DEC VT340 color map', 'libsixel built-in vt340color')
    return svg_document(width, height, 'Standard fixed palettes',
                        'Swatches of the xterm 256-color palette, the 216-color web-safe '
                        'palette, and the 16-entry VT340 color map, read from libsixel '
                        'built-in tables where available.', body)


def block_palette():
    rng = np.random.default_rng(20261006)
    width, height = 760, 250
    c0 = np.array([46, 92, 160])
    c1 = np.array([238, 190, 92])
    colors = [c0, c1, (2 * c0 + c1) / 3, (c0 + 2 * c1) / 3]
    order = [0, 2, 3, 1]
    indices = np.clip(np.round(np.add.outer(np.arange(4), np.arange(4)) / 2
                               + rng.integers(-1, 2, (4, 4)) * 0.4), 0, 3).astype(int)
    body = []
    cell = 40
    body.append(svg_text(100, 30, '4 × 4 block of indices', 15, INK, 700, 'middle'))
    for y in range(4):
        for x in range(4):
            value = order[indices[y, x]]
            body.append(f'<rect x="{20 + x * cell}" y="{45 + y * cell}" width="{cell}" '
                        f'height="{cell}" fill="#ffffff" stroke="{MUTED}"/>')
            body.append(svg_text(20 + x * cell + cell / 2, 45 + y * cell + 26, str(value), 15,
                                 INK, 700, 'middle'))
    body.append(svg_text(330, 30, 'Palette of this block', 15, INK, 700, 'middle'))
    labels = ('0: endpoint C0', '1: endpoint C1', '2: ⅔ C0 + ⅓ C1', '3: ⅓ C0 + ⅔ C1')
    for index, (color, label) in enumerate(zip(colors, labels)):
        y = 50 + index * 38
        body.append(f'<rect x="250" y="{y}" width="44" height="30" fill="{hex_color(color)}" '
                    f'stroke="{MUTED}"/>')
        body.append(svg_text(304, y + 21, label, 14, INK))
    body.append(svg_text(610, 30, 'Decoded block', 15, INK, 700, 'middle'))
    for y in range(4):
        for x in range(4):
            value = order[indices[y, x]]
            body.append(f'<rect x="{530 + x * cell}" y="{45 + y * cell}" width="{cell}" '
                        f'height="{cell}" fill="{hex_color(colors[value])}"/>')
    body.append(svg_text(width / 2, 238, 'Two stored colors and sixteen 2-bit indices: 64 '
                         'bits for 16 pixels, every 4 × 4 block with its own palette.', 14,
                         MUTED, 400, 'middle'))
    return svg_document(width, height, 'Block palette texture compression',
                        'A four by four block stores two endpoint colors and a two-bit '
                        'index per pixel; the two other palette colors are interpolated '
                        'between the endpoints.', body)


FIGURES = {
    'indexed-image.svg': indexed_image,
    'timeline.svg': timeline,
    'palette-sizes.svg': palette_sizes,
    'standard-palettes.svg': standard_palettes,
    'block-palette.svg': block_palette,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {name: draw() for name, draw in FIGURES.items()}
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_palette_concept_figures.py',
        'plot_dependency': 'matplotlib 3.10, numpy, Pillow',
        'builtin_tables': {'xterm256': 'src/dither.c pal_xterm256',
                           'vt340color': 'src/dither.c pal_vt340_color'},
        'web_safe_levels': [0, 51, 102, 153, 204, 255],
        'palette_sizes': 'images/snake.png at 240 x 180, Pillow median cut, '
                         'Floyd-Steinberg dithering',
        'timeline': [{'year': y, 'event': t, 'detail': d} for y, t, d in EVENTS],
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
