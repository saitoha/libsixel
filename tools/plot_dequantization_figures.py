#!/usr/bin/env python3
"""Generate schematic dequantization guides for desktop and mobile readers."""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/functionality/dequantization-figures'
INK = '#183247'
MUTED = '#4c6070'
BLUE = '#17618c'
GOLD = '#855012'
METHODS = {
    'none': ('Keep each pixel', 'none', [],
             ['Read the center pixel.', 'Keep its decoded color.',
              'No neighboring colors are mixed.'],
             ['Useful when every pixel matters:', 'pixel art, text, or inspection.']),
    'full': ('Look all around', 'k_undither = lso_undither:Vfs',
             [0, 1, 2, 3, 5, 6, 7, 8],
             ['Consider all eight neighbors.', 'Use the image palette to decide',
              'how much each color contributes.'],
             ['Optional edge protection (-e)', 'can keep strong edges sharper.']),
    'light': ('Look above and left', 'lso_undither:Vlight', [0, 1, 2, 3],
              ['Use only these four neighbors.', 'Use the same palette-aware idea',
               'with fewer samples per pixel.'],
              ['No edge-protection stage.', 'The result can differ from Vfs.']),
    'selective': ('Mix only nearby colors', 'selective_blur',
                  [0, 1, 3, 6, 7],
                  ['Compare each neighbor to center.', 'Mix colors within the threshold.',
                   'Leave distant colors out.'],
                  ['Higher threshold: more colors mix.', 'The neighborhood stays 3 x 3.']),
}


def text(x, y, value, size=17, color=INK, weight=400, anchor='start'):
    """Escape every label so standalone SVGs remain XML-safe."""
    return (f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" '
            f'font-weight="{weight}" text-anchor="{anchor}">'
            f'{html.escape(value)}</text>')


def rect(x, y, width, height, fill, stroke='none', radius=10, sw=1):
    return (f'<rect x="{x}" y="{y}" width="{width}" height="{height}" '
            f'rx="{radius}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{sw}"/>')


def lines(x, y, values, size=17, color=INK, step=26):
    return ''.join(text(x, y + i * step, v, size, color)
                   for i, v in enumerate(values))


def grid(kind, x, y):
    """Show location, not invented output colors or similarity magnitudes."""
    active = METHODS[kind][2]
    body = []
    for i in range(9):
        cx, cy = x + (i % 3) * 80, y + (i // 3) * 70
        center = i == 4
        used = i in active
        fill = '#fff0d6' if center else '#e5f3fa' if used else '#f1f3f5'
        stroke = GOLD if center else BLUE if used else '#c8d0d7'
        label = 'CENTER' if center else 'USE' if used else 'SKIP'
        if kind == 'selective' and used:
            label = 'NEAR'
        if kind == 'selective' and not used and not center:
            label = 'FAR'
        body.append(rect(cx, cy, 72, 62, fill, stroke, 7, 3 if center else 1))
        body.append(text(cx + 36, cy + 35, label, 13,
                         GOLD if center else BLUE if used else MUTED,
                         700, 'middle'))
    return ''.join(body)


def evidence(kind, x, y, width):
    """Render the method-specific decision, without a false quality ranking."""
    body = [rect(x, y, width, 104, '#f3f7fa')]
    if kind == 'none':
        for dx, label in [(18, 'Input'), (width - 88, 'Output')]:
            body.append(rect(x + dx, y + 16, 64, 35, '#bd6a34', radius=5))
            body.append(text(x + dx + 32, y + 76, label, 16, anchor='middle'))
        body.append(text(x + width / 2, y + 41, '=', 25, anchor='middle'))
    elif kind in ('full', 'light'):
        body.append(text(x + 15, y + 25, 'Palette = colors stored in the image', 15))
        for j, color in enumerate(['#24577a', '#69a6c3', '#d3e7e9', '#bd6a34', '#e6bd7f']):
            body.append(rect(x + 15 + j * 48, y + 37, 40, 24, color, radius=4))
        body.append(text(x + 15, y + 86, 'The palette affects mixing weights.', 15))
    else:
        body.append(text(x + 15, y + 24, 'Example: threshold 24, red channel', 15))
        body.append(text(x + 15, y + 52, '100 + nearby 112  →  104', 21, BLUE, 700))
        body.append(text(x + 15, y + 81, 'Center weight 4; neighbor weight 2.', 14))
    return ''.join(body)


def figure(kind, mobile):
    title, option, _, steps, note = METHODS[kind]
    width, height = (380, 850) if mobile else (960, 510)
    body = [rect(0, 0, width, height, '#ffffff', radius=0),
            text(24, 37, title, 26, weight=700),
            text(24, 65, option, 16, BLUE)]
    gx, gy = (74, 115) if mobile else (66, 139)
    body += [text(24 if mobile else 42, 101 if mobile else 112,
                  '1  Which pixels are considered?', 18, weight=700),
             grid(kind, gx, gy)]
    body.append(lines(24 if mobile else 42, gy + 238,
                      ['CENTER = the pixel being updated',
                       'Labels show the role of each square.'], 15, MUTED, 23))
    if kind == 'selective':
        body.append(text(24 if mobile else 42, gy + 285,
                         'NEAR / FAR is one possible pattern.', 15, MUTED))
    if mobile:
        tx, ty, panel_width = 24, 440, 332
    else:
        tx, ty, panel_width = 460, 112, 458
    body.append(text(tx, ty, '2  How is the center updated?', 18, weight=700))
    body.append(lines(tx, ty + 38, steps))
    body.append(evidence(kind, tx, ty + 112, panel_width))
    body.append(lines(tx, ty + 247, note, 16, MUTED, 25))
    body.append(lines(24, height - 47,
                      ['Concept diagram • not a measured before/after image.',
                       'Only the center is updated here; repeat across the image.'],
                      12 if mobile else 15, MUTED, 21))
    desc = (f'{title}. {" ".join(steps)} {" ".join(note)} '
            'The grid marks the center and eligible neighbor positions. '
            'This is a schematic, not a measured image comparison.')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
            f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
            'aria-labelledby="title desc" font-family="Arial, sans-serif">\n'
            f'<title id="title">{html.escape(title)}</title>\n'
            f'<desc id="desc">{html.escape(desc)}</desc>\n'
            + '\n'.join(body) + '\n</svg>\n')


def document(width, height, title, desc, body):
    """Keep the explanatory figures usable as standalone accessible assets."""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" '
            f'height="{height}" viewBox="0 0 {width} {height}" role="img" '
            'aria-labelledby="title desc" font-family="Arial, sans-serif">\n'
            f'<title id="title">{html.escape(title)}</title>\n'
            f'<desc id="desc">{html.escape(desc)}</desc>\n'
            + '\n'.join(body) + '\n</svg>\n')


def palette_clue(mobile):
    """Hold the observed pair fixed while changing its available palette."""
    width, height = (380, 1060) if mobile else (960, 610)
    title = 'Was the middle color available?'
    body = [rect(0, 0, width, height, '#ffffff', radius=0),
            text(20, 36, title, 23, weight=700),
            text(20, 64, 'Same pixels. Different palette evidence.', 16, MUTED)]
    for present in (False, True):
        x = 20 if mobile else 24 + int(present) * 472
        y = 85 + int(present) * 470 if mobile else 90
        pw = 340 if mobile else 440
        body.append(rect(x, y, pw, 450, '#f6f8fa', '#c8d0d7'))
        body.append(text(x + 16, y + 30,
                         '2  Middle color available' if present else
                         '1  Middle color unavailable', 19, weight=700))
        body.append(text(x + 16, y + 61, 'Observed neighboring pixels:', 16))
        for j, (v, name) in enumerate([(80, 'A = 80'), (160, 'B = 160')]):
            sx = x + 30 + j * 142
            body.append(rect(sx, y + 77, 102, 45, f'rgb({v},{v},{v})', radius=4))
            body.append(text(sx + 51, y + 147, name, 16, anchor='middle'))
        body.append(text(x + 16, y + 190, 'Palette entries on a grayscale axis', 15))
        left, right = x + 42, x + pw - 42
        middle = (left + right) / 2
        body.append(f'<path d="M {left} {y+245} H {right}" stroke="{MUTED}"/>')
        for sx, v in [(left, 80), (middle, 120), (right, 160)]:
            available = v != 120 or present
            body.append(rect(sx - 10, y + 235, 20, 20,
                             f'rgb({v},{v},{v})' if available else '#ffffff',
                             BLUE if v == 120 else MUTED, 0, 2))
            body.append(text(sx, y + 280, str(v), 16, anchor='middle'))
        body.append(text(middle, y + 216, 'M: midpoint', 15, BLUE, anchor='middle'))
        body.append(text(middle, y + 309,
                         '120 is a stored palette color' if present else
                         '120 is missing from the palette', 15, BLUE, anchor='middle'))
        body.append(lines(x + 16, y + 355,
                          ['The encoder could have used 120.',
                           'A/B may be a real boundary.',
                           'Suppress this pair\'s mixing.'] if present else
                          ['A/B may stand in for missing 120.',
                           'An intermediate color is plausible.',
                           'Allow this pair to mix.'], 16, INK, 26))
    body.append(text(20, height - 17,
                     'Palette clue only; spatial gradients also protect edges.',
                     12 if mobile else 16, MUTED))
    return document(width, height, title,
                    'The observed A=80 and B=160 do not change. A midpoint '
                    'entry at 120 changes the inference from a plausible '
                    'missing shade to evidence for a boundary.', body)


def gradient_response(patch):
    """Evaluate the upstream center Prewitt response for opaque grayscale."""
    gray = [4 * value for value in patch]
    gx = gray[2] - gray[0] + gray[5] - gray[3] + gray[8] - gray[6]
    gy = sum(gray[6:9]) - sum(gray[0:3])
    return gx, gy, (gx * gx + gy * gy) // 256


def gradient_clue(mobile):
    """Compare cancellation in alternating dots with a coherent boundary."""
    width, height = (380, 1120) if mobile else (960, 635)
    title = 'Dots or a coherent boundary?'
    body = [rect(0, 0, width, height, '#ffffff', radius=0),
            text(20, 36, title, 23, weight=700),
            text(20, 64, 'Same shades. Different spatial structure.', 16, MUTED)]
    patches = [[80, 160, 80, 160, 80, 160, 80, 160, 80],
               [80, 80, 160, 80, 80, 160, 80, 80, 160]]
    for i, patch in enumerate(patches):
        x = 20 if mobile else 24 + i * 472
        y = 85 + i * 475 if mobile else 90
        pw = 340 if mobile else 440
        body.append(rect(x, y, pw, 455, '#f6f8fa', '#c8d0d7'))
        body.append(text(x + 16, y + 31,
                         'Alternating dots' if i == 0 else 'A vertical step',
                         20, weight=700))
        sx = x + (pw - 180) / 2
        for j, v in enumerate(patch):
            cx, cy = sx + j % 3 * 60, y + 55 + j // 3 * 60
            body.append(rect(cx, cy, 56, 56, f'rgb({v},{v},{v})',
                             GOLD if j == 4 else 'none', 3, 3))
            body.append(text(cx + 28, cy + 34, str(v), 16,
                             '#ffffff' if v == 80 else '#111111',
                             anchor='middle'))
        gx, gy, response = gradient_response(patch)
        body.append(text(x + 16, y + 263, f'Horizontal sum: {gx}; vertical: {gy}', 15))
        body.append(text(x + 16, y + 300, f'Gradient response = {response}', 21, BLUE, 700))
        body.append(lines(x + 16, y + 339,
                          ['Opposite sides cancel here.',
                           'Center weight stays at 8.',
                           'Palette evidence decides mixing.'] if i == 0 else
                          ['The sides differ consistently.',
                           '3600 exceeds the threshold 256.',
                           'Keep the center unchanged.'], 16, INK, 26))
    footer = (['Calculated from brightness R + 2G + B.',
               'Upstream protection. In libsixel, -e 100',
               'selects these thresholds.'] if mobile else
              ['Calculated from brightness R + 2G + B.',
               'Upstream protection; libsixel needs -e 100 for these thresholds.'])
    body.append(lines(20, height - (67 if mobile else 55),
                      footer, 12 if mobile else 16, MUTED, 22))
    return document(width, height, title,
                    'At the center, an 80/160 checkerboard has response 0; '
                    'an 80/160 vertical step has response 3600. The upstream '
                    'gradient gate protects the step from mixing.', body)


def rgb_geometry(mobile):
    """Show exact RGB geometry in a constant-blue plane, with equal scales."""
    width, height = (380, 1590) if mobile else (1080, 680)
    body = [rect(0, 0, width, height, '#ffffff', radius=0),
            text(20, 35, 'The nearest color need not lie on A–B',
                 18 if mobile else 27, weight=700),
            text(20, 64, 'RGB slice: blue = 128 in every point', 16, MUTED)]
    examples = [(144, 144, 0, 0), (152, 176, 1, 5), (160, 192, 6, 8)]
    for i, (red, green, upstream, local) in enumerate(examples):
        x, y = (20, 90 + i * 465) if mobile else (20 + i * 355, 90)
        pw = 340
        body.append(rect(x, y, pw, 450, '#f6f8fa', '#c8d0d7'))
        body.append(text(x + 16, y + 30, f'{i+1}  P = ({red}, {green}, 128)', 19, weight=700))
        def pos(r, g):
            return x + 38 + (r - 32) * 1.1, y + 263 - (g - 96) * 1.1
        a, b, m, p = pos(64,128), pos(192,128), pos(128,128), pos(red,green)
        # Axes and distances share one pixel-per-unit scale in both directions.
        body.append(f'<path d="M {x+32} {y+65} V {y+315} H {x+315}" '
                    'fill="none" stroke="#c8d0d7"/>')
        body.append(text(x+39,y+70,'Green ↑',13,MUTED))
        body.append(text(x+244,y+335,'Red →',13,MUTED))
        body.append(f'<circle cx="{m[0]}" cy="{m[1]}" r="70.4" '
                    'fill="none" stroke="#91a8b7" stroke-dasharray="3 4"/>')
        body.append(f'<path d="M {a[0]} {a[1]} L {p[0]} {p[1]} '
                    f'L {b[0]} {b[1]} Z" fill="#e5f3fa" '
                    'fill-opacity="0.55" stroke="#91a8b7"/>')
        body.append(f'<path d="M {m[0]} {m[1]} L {p[0]} {p[1]}" '
                    f'stroke="{BLUE}" stroke-width="3"/>')
        body.append(f'<path d="M {a[0]} {a[1]} L {m[0]} {m[1]}" '
                    f'stroke="{GOLD}" stroke-width="3"/>')
        for r,g in [(40,220),(225,215)]:
            px,py=pos(r,g)
            body.append(f'<path d="M {px-4} {py-4} l 8 8 m -8 0 l 8 -8" '
                        'stroke="#607586" stroke-width="2"/>')
        for label, point, color in [('A',a,'rgb(64,128,128)'),
                                     ('B',b,'rgb(192,128,128)'),
                                     ('P',p,f'rgb({red},{green},128)')]:
            px,py=point
            body.append(rect(px-6,py-6,12,12,color,INK,0,1))
            body.append(text(px+(12 if label=='P' else 0),
                             py+(-12 if label=='P' else 26),label,17,INK,700,
                             'start' if label=='P' else 'middle'))
        body.append(f'<circle cx="{m[0]}" cy="{m[1]}" r="5" '
                    f'fill="white" stroke="{GOLD}" stroke-width="2"/>')
        body.append(text(m[0],m[1]+26,'M',17,GOLD,700,'middle'))
        q=(red-128)**2+(green-128)**2
        body.append(text(x+16,y+360,f'q/d = {q}/4096 = {q/4096:g}',18,BLUE,700))
        body.append(text(x+16,y+391,f'Neighbor weight: upstream {upstream}',16))
        body.append(text(x+16,y+419,f'libsixel {local} at -S 100',16))
    footer = ['A=(64,128,128); B=(192,128,128).',
              'M=(128,128,128); d=|M−A|²; q=|M−P|².',
              'Dashed circle: |M−A|. Crosses: other colors.',
              'P is the closest other palette entry to M.']
    body.append(lines(20,height-91,footer,12 if mobile else 15,MUTED,22))
    return document(width,height,'Four-point RGB geometry',
                    'A and B and their midpoint M are fixed. Three different '
                    'palettes place the nearest other color P off the A-B line. '
                    'The squared midpoint distances determine the pair weight. '
                    'The dashed circle has radius M-A. This is an exact '
                    'constant-blue slice, not a projection with distorted distances.',body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {f'{kind}-{layout}.svg': figure(kind, layout == 'mobile')
              for kind in METHODS for layout in ('wide', 'mobile')}
    for layout in ('wide', 'mobile'):
        assets[f'rgb-geometry-{layout}.svg'] = rgb_geometry(layout == 'mobile')
        assets[f'palette-clue-{layout}.svg'] = palette_clue(layout == 'mobile')
        assets[f'gradient-clue-{layout}.svg'] = gradient_clue(layout == 'mobile')
    assets['figures.json'] = json.dumps({
        'kind': 'schematic with calculated grayscale examples',
        'upstream_revision': '844241504c7f2b224c67761de277c2bb5c56ab81',
        'gradient_examples': {'checkerboard': 0, 'vertical_step': 3600},
        'rgb_geometry': {'A': [64,128,128], 'B': [192,128,128],
                         'M': [128,128,128],
                         'nearest_colors': [[144,144,128], [152,176,128],
                                            [160,192,128]],
                         'other_colors': [[40,220,128], [225,215,128]]},
        'inference_layouts': {
            'rgb-geometry': {'wide': [1080,680], 'mobile': [380,1590]},
            'palette-clue': {'wide': [960, 610], 'mobile': [380, 1060]},
            'gradient-clue': {'wide': [960, 635], 'mobile': [380, 1120]},
        },
        'source': 'src/decoder.c',
        'color_roles': {'gold': 'center', 'blue': 'considered neighbor',
                        'gray': 'unused or excluded neighbor'},
        'methods': {k: {'title': v[0], 'option': v[1],
                        'illustrated_neighbor_positions': v[2]}
                    for k, v in METHODS.items()},
        'layouts': {'wide': [960, 510], 'mobile': [380, 850]},
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
