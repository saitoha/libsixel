#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate a reproducible DIN99d nearest-color example as static SVG."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
from evaluate import deltaE00, rgb_to_lab
from plot_colorspace_figures import ROOT, BLUE, GOLD, INK, MUTED, arrow_marker, connector, document, rect, stage, text

ROUTE = {
    'input': ('sRGB', ['Encoded RGB values']),
    'linear': ('Linear RGB', ['Decode sRGB curve']),
    'xyz': ('XYZ · D65', ['RGB → XYZ matrix']),
    'labprime': (['Corrected', 'Lab-like values'], ['Correct X + white', 'Apply Lab functions']),
    'din': ('DIN99d', ['Reshape L / chroma', 'Divide all axes by 100']),
}
ROUTE_EDGES = list(zip(list(ROUTE)[:-1], list(ROUTE)[1:]))

COLORS = {'target': [128, 128, 128], 'A': [135, 124, 128], 'B': [136, 136, 136]}
MATRIX = np.array([[.4124564, .3575761, .1804375],
                   [.2126729, .7151522, .0721750],
                   [.0193339, .1191920, .9503041]])
WHITE = np.array([.95047, 1., 1.08883])


def xyz_to_din99d(xyz, white):
    """Cui et al. (2002), equation (5), in native (unscaled) units."""
    sample = np.array(xyz, dtype=float, copy=True)
    reference = np.array(white, dtype=float, copy=True)
    sample[0] = 1.12 * sample[0] - .12 * sample[2]
    reference[0] = 1.12 * reference[0] - .12 * reference[2]
    t = sample / reference
    f = np.where(t > (6 / 29) ** 3, np.cbrt(t), t / (3 * (6 / 29) ** 2) + 4 / 29)
    lightness, a, b = 116 * f[1] - 16, 500 * (f[0] - f[1]), 200 * (f[1] - f[2])
    angle = math.radians(50)
    e = a * math.cos(angle) + b * math.sin(angle)
    z = 1.14 * (-a * math.sin(angle) + b * math.cos(angle))
    chroma = 22.5 * math.log1p(.06 * math.hypot(e, z))
    hue = math.atan2(z, e) + angle
    return np.array([325.22 * math.log1p(.0036 * lightness),
                     chroma * math.cos(hue), chroma * math.sin(hue)])


def example():
    # An independent appendix check prevents a paired forward/inverse omission.
    expected = [[25.5720, -.9627, -30.8731], [25.9071, -3.5163, -29.1561]]
    samples = [[4.96, 3.72, 19.59], [4.6651, 3.81, 17.7848]]
    converted = [xyz_to_din99d(v, [94.81, 100., 107.33]) for v in samples]
    if not np.allclose(converted, expected, rtol=0, atol=.00005):
        raise SystemExit('DIN99d appendix reference mismatch')
    rgb = {k: np.array(v, dtype=float) / 255 for k, v in COLORS.items()}
    din = {}
    for name, value in rgb.items():
        linear = np.where(value <= .04045, value / 12.92, ((value + .055) / 1.055) ** 2.4)
        din[name] = xyz_to_din99d(MATRIX @ linear, WHITE) / 100
    distances = {space: {k: float(np.linalg.norm(coords[k] - coords['target']))
                        for k in ('A', 'B')}
                 for space, coords in [('gamma', rgb), ('din99d', din)]}
    errors = {k: float(deltaE00(rgb_to_lab(rgb['target']), rgb_to_lab(rgb[k])))
              for k in ('A', 'B')}
    if not (distances['gamma']['A'] < distances['gamma']['B']
            and distances['din99d']['B'] < distances['din99d']['A']
            and errors['B'] < errors['A']):
        raise SystemExit('The nearest-color example changed')
    return {'rgb8': COLORS, 'din99d_divided_by_100': {k: v.tolist() for k, v in din.items()},
            'distances': distances, 'delta_e00': errors,
            'appendix_native_coordinates': [v.tolist() for v in converted]}


def draw(mobile, data):
    width, height = (380, 1110) if mobile else (960, 730)
    body = [text(20, 36, 'Which palette color is closer?', 23 if mobile else 30, weight=700),
            text(20, 65, 'A small color cast can matter more than brightness.',
                 13 if mobile else 18, MUTED)]
    for i, key in enumerate(('target', 'A', 'B')):
        x, y, w = (20 + i * 118, 96, 104) if mobile else (24 + i * 312, 96, 288)
        color = '#%02x%02x%02x' % tuple(COLORS[key])
        label = {'target': 'Target gray', 'A': 'A · Tinted', 'B': 'B · Brighter'}[key]
        body.extend([rect(x, y, w, 82, color, '#c4ced6', 5),
                     text(x, y + 105, label, 14 if mobile else 19, weight=700),
                     text(x, y + 129, color, 14 if mobile else 17, MUTED)])
    for i, (space, name, chosen, accent) in enumerate([
            ('gamma', 'sRGB encoded distance', 'A', GOLD),
            ('din99d', 'DIN99d distance (÷100)', 'B', BLUE)]):
        x, y, w = (20, 260 + i * 392, 340) if mobile else (24 + i * 472, 264, 440)
        body.extend([rect(x, y, w, 356, '#f5f7f9', '#c4ced6', 10),
                     text(x + 16, y + 33, name, 20 if mobile else 23, accent, 700),
                     text(x + 16, y + 60, 'Shorter distance wins within this panel.',
                          13 if mobile else 16, MUTED)])
        # Equal zero-based axes make the numbers inspectable. Distances still
        # belong to different coordinate systems; their cross-panel ratio
        # does not describe perceptual improvement.
        bar_width = w - 104
        for j, key in enumerate(('A', 'B')):
            top = y + 90 + j * 44
            distance = data['distances'][space][key]
            body.extend([text(x + 16, top + 19, key, 17, weight=700),
                         rect(x + 43, top, distance / .08 * bar_width, 24,
                              accent if key == chosen else '#b6c1c9'),
                         text(x + w - 12, top + 18, f'{distance:.4f}',
                              14, INK, anchor='end')])
        body.append(text(x + 16, y + 190, f'Choose {chosen}', 19, accent, 700))
        fill = '#%02x%02x%02x' % tuple(COLORS[chosen])
        body.extend([rect(x + 16, y + 208, w - 32, 82, fill, radius=5),
                     rect(x + w / 2 - 32, y + 228, 64, 42, '#808080'),
                     text(x + 16, y + 316, f"Selected color: ΔE00 = {data['delta_e00'][chosen]:.2f}",
                          17 if mobile else 19, weight=700),
                     text(x + 16, y + 341, 'Center inset = target; field = selected color.',
                          13 if mobile else 16, MUTED)])
    lines = ['Constructed two-candidate example; exact formulas, no dithering.',
             'Distance scales belong to their own spaces. ΔE00 is a separate check.',
             'This example explains the choice; it does not rank whole-image quality.']
    if mobile:
        lines = ['Constructed example; exact formulas, no dithering.',
                 'Each distance belongs to its own space.',
                 'ΔE00 is a separate check of the selected color.',
                 'Whole-image results depend on content and settings.']
    for i, line in enumerate(lines):
        body.append(text(20, height - 15 - (len(lines) - 1 - i) * 22, line,
                         13 if mobile else 17, MUTED))
    return document(width, height, 'DIN99d can avoid a tinted nearest-color choice',
                    'Target #808080 has two candidate palette colors: tinted #877c80 '
                    'and brighter neutral #888888. Encoded sRGB Euclidean distance '
                    'chooses the tinted candidate, whose CIEDE2000 error is 6.39. '
                    'DIN99d chooses the neutral candidate, whose CIEDE2000 error is 2.95. '
                    'The inset is the target gray. This is a constructed analytical '
                    'example before SIXEL percentage rounding, not an image benchmark.', body)


def route(mobile):
    width, height = (380, 890) if mobile else (960, 385)
    body = [arrow_marker(),
            text(20, 36, 'DIN99d begins before Lab', 24 if mobile else 30, weight=700),
            text(20, 65, 'The sample and its reference white change together.',
                 13 if mobile else 18, MUTED)]
    boxes = {key: ((40, 98 + i * 137, 300, 100) if mobile
                   else (12 + i * 190, 108, 176, 136))
             for i, key in enumerate(ROUTE)}
    for source, target in ROUTE_EDGES:
        x, y, w, h = boxes[source]
        tx, ty, tw, th = boxes[target]
        points = [(x + w / 2, y + h + 1), (tx + tw / 2, ty - 3)] if mobile else [
            (x + w + 1, y + h / 2), (tx - 3, ty + th / 2)]
        body.append(connector(points, source, target))
    for key, (heading, details) in ROUTE.items():
        body.append(stage(key, boxes[key], heading, details, mobile,
                          hub=key in ('labprime', 'din')))
    lines = ["Sample: X′ = 1.12X − 0.12Z.  White: X′n = 1.12Xn − 0.12Zn.",
             'The inverse restores ordinary X before the XYZ → linear-RGB matrix.',
             'A shared coordinate divisor preserves the relative weights of L, a and b.',
             'Schematic stages; a separate XYZ image buffer is not implied.']
    if mobile:
        lines = ['Sample: X′ = 1.12X − 0.12Z',
                 'White: X′n = 1.12Xn − 0.12Zn',
                 'The inverse restores X before returning to RGB.',
                 'One divisor preserves relative axis weights.',
                 'Schematic stages, not image-buffer allocations.']
    for i, line in enumerate(lines):
        body.append(text(20, height - 18 - (len(lines) - i - 1) * 25, line,
                         14 if mobile else 18, MUTED))
    return document(width, height, 'The corrected sRGB to DIN99d conversion route',
                    'sRGB decodes into linear RGB, which maps to D65 XYZ. The sample '
                    'and reference-white X become 1.12 X minus 0.12 Z before the '
                    'Lab-like stage. DIN99d reshapes lightness and chroma and stores '
                    'every axis divided by 100. The inverse undoes the X correction '
                    'before ordinary XYZ-to-RGB conversion.', body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'docs/concepts/din99d-figures')
    args = parser.parse_args()
    data = example()
    assets = {f'nearest-color-{layout}.svg': draw(layout == 'mobile', data)
              for layout in ('wide', 'mobile')}
    assets.update({f'conversion-route-{layout}.svg': route(layout == 'mobile')
                   for layout in ('wide', 'mobile')})
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_din99d_figures.py', 'kinds': {'nearest-color': 'constructed analytical example',
                  'conversion-route': 'schematic transform stages, not buffer allocations'},
        'route': {'nodes': ROUTE, 'edges': ROUTE_EDGES},
        'dependencies': ['numpy', 'Pillow'], 'example': data,
        'sources': ['https://doi.org/10.1002/col.10066', 'tools/evaluate.py', 'src/colorspace.c'],
        'notes': ['DIN99d reference uses exact cube roots; the C implementation uses a lookup table.',
                  'All three DIN99d axes share divisor 100.',
                  'CIEDE2000 is computed independently in ordinary D65 CIELAB.',
                  'Swatches precede SIXEL palette percentage rounding.'],
    }, indent=2) + '\n'
    if not args.check:
        args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, content in assets.items():
        path = args.output_dir / name
        if args.check:
            if not path.exists() or path.read_text() != content:
                raise SystemExit(f'Stale figure: {path}')
        else:
            path.write_text(content)
    print(f'{"Verified" if args.check else "Generated"} {len(assets)} DIN99d assets')


if __name__ == '__main__':
    main()
