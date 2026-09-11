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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    assets = {f'{kind}-{layout}.svg': figure(kind, layout == 'mobile')
              for kind in METHODS for layout in ('wide', 'mobile')}
    assets['figures.json'] = json.dumps({
        'kind': 'schematic, not measured output',
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
