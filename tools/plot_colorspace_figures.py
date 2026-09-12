#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate static color-space figures; use Matplotlib 3.10.3 to reproduce."""

from __future__ import annotations

import argparse
import html
import io
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/concepts/colorspace-figures'
INK = '#183247'
MUTED = '#4c6070'
BLUE = '#17618c'
GOLD = '#996015'
NS = 'http://www.w3.org/2000/svg'
SPACES = ('sRGB', 'linear RGB', 'OKLab')
PAIRS = {'black-white': ((0, 0, 0), (1, 1, 1)),
         'blue-red': ((0, 0, 1), (1, 0, 0))}
PRIMARIES = {'sRGB': [[.64, .33], [.30, .60], [.15, .06]],
             'Display P3': [[.68, .32], [.265, .69], [.15, .06]]}
WHITES = {'D65': [.3127, .3290], 'D50': [.3457, .3585]}
ROUTES = [
    {'title': 'A · No conversion needed',
     'condition': 'No CMS, no mixing or resize; gamma work',
     'nodes': ['sRGB input', 'sRGB work'],
     'edges': ['keep the same coordinates'],
     'note': 'No linear image buffer is needed for this route.'},
    {'title': 'B · Mix light only when needed',
     'condition': 'Linear composition / default resize; gamma work',
     'nodes': ['sRGB input', 'linear RGB', 'Mix / resize', 'sRGB work'],
     'edges': ['decode sRGB', 'linear-light math', 'encode sRGB'],
     'note': 'A linear loader result can skip the first conversion.'},
    {'title': 'C · Enter a perceptual space',
     'condition': 'OKLab palette processing, even without mixing',
     'nodes': ['sRGB input', 'linear RGB', 'LMS', 'OKLab'],
     'edges': ['decode sRGB', 'XYZ matrices combined', 'cube roots + matrix'],
     'note': 'LMS approximates cone responses; a separate XYZ image is unnecessary.'},
    {'title': 'D · Connect an ICC source profile',
     'condition': 'Conceptual source-to-linear-sRGB normalization',
     'nodes': ['Input values', 'PCS · D50', 'XYZ · D65', 'linear RGB', 'Mix light'],
     'edges': ['source profile', 'XYZ + adaptation', 'RGB matrix', 'linear-light math'],
     'note': 'PCS uses XYZ or Lab; it connects profiles, not processing stages.'},
]


def text(x, y, value, size=18, color=INK, weight=400, anchor='start'):
    return (f'<text x="{x:g}" y="{y:g}" font-size="{size}" '
            f'fill="{color}" font-weight="{weight}" text-anchor="{anchor}">'
            f'{html.escape(value)}</text>')


def rect(x, y, width, height, fill, stroke='none', radius=0):
    return (f'<rect x="{x:g}" y="{y:g}" width="{width:g}" '
            f'height="{height:g}" rx="{radius}" fill="{fill}" '
            f'stroke="{stroke}"/>')


def document(width, height, title, description, body):
    return (f'<svg xmlns="{NS}" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}" role="img" '
            'aria-labelledby="title desc" font-family="Arial, sans-serif">\n'
            f'<title id="title">{html.escape(title)}</title>\n'
            f'<desc id="desc">{html.escape(description)}</desc>\n'
            + rect(0, 0, width, height, '#ffffff') + '\n'
            + '\n'.join(body) + '\n</svg>\n')


def decode(value):
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def encode(value):
    return value * 12.92 if value <= .0031308 else 1.055 * value ** (1 / 2.4) - .055


def matrix(rows, vector):
    return [sum(a * b for a, b in zip(row, vector)) for row in rows]


def oklab(rgb):
    lms = matrix([[.4122214708, .5363325363, .0514459929],
                  [.2119034982, .6806995451, .1073969566],
                  [.0883024619, .2817188376, .6299787005]],
                 [decode(v) for v in rgb])
    return matrix([[.2104542553, .7936177850, -.0040720468],
                   [1.9779984951, -2.4285922050, .4505937099],
                   [.0259040371, .7827717662, -.8086757660]],
                  [math.copysign(abs(v) ** (1 / 3), v) for v in lms])


def from_oklab(lab):
    lms = matrix([[1, .3963377774, .2158037573],
                  [1, -.1055613458, -.0638541728],
                  [1, -.0894841775, -1.2914855480]], lab)
    linear = matrix([[4.0767416621, -3.3077115913, .2309699292],
                     [-1.2684380046, 2.6097574011, -.3413193965],
                     [-.0041960863, -.7034186147, 1.7076147010]],
                    [v ** 3 for v in lms])
    return [encode(v) for v in linear]


def mix(pair, space, fraction):
    a, b = PAIRS[pair]
    if space == 'linear RGB':
        a, b = [decode(v) for v in a], [decode(v) for v in b]
    elif space == 'OKLab':
        a, b = oklab(a), oklab(b)
    result = [x * (1 - fraction) + y * fraction for x, y in zip(a, b)]
    if space == 'linear RGB':
        result = [encode(v) for v in result]
    elif space == 'OKLab':
        result = from_oklab(result)
    return result


def rgb8(values):
    # Round half up to match the byte boundary used in the prose examples.
    return [int(max(0, min(1, v)) * 255 + .5) for v in values]


def color(values):
    return 'rgb(' + ','.join(str(v) for v in rgb8(values)) + ')'


def mixing(mobile):
    width, height = (380, 1100) if mobile else (960, 650)
    title = 'The same 50% mix. Three different results.'
    body = [text(20, 34, 'What does a 50% mix mean?' if mobile else title,
                 23 if mobile else 28, weight=700),
            text(20, 63, 'Same endpoints; different interpolation coordinates.',
                 14 if mobile else 18, MUTED)]
    explanations = ['Mix encoded values', 'Mix amounts of light', 'Interpolate perceptual coordinates']
    for column, pair in enumerate(PAIRS):
        x, y, panel_width = (20, 90 + column * 466, 340) if mobile else (24 + column * 472, 95, 440)
        body.append(rect(x, y, panel_width, 442, '#f5f7f9', radius=10))
        body.append(text(x + 16, y + 32, 'Black + white' if pair == 'black-white' else 'Blue + red', 22, weight=700))
        for row, space in enumerate(SPACES):
            top = y + 60 + row * 122
            body.append(f'<g data-role="interpolation" data-space="{space}" data-pair="{pair}">')
            body.append(text(x + 16, top + 3, space, 19, weight=700))
            body.append(text(x + 16, top + 25, explanations[row], 15 if mobile else 17, MUTED))
            identifier = f'{pair}-{row}'
            stops = ''.join(f'<stop offset="{i}%" stop-color="{color(mix(pair, space, i / 100))}"/>' for i in range(101))
            body.append(f'<defs><linearGradient id="{identifier}" color-interpolation="sRGB">{stops}</linearGradient></defs>')
            gradient_width = panel_width - 108
            body.append(rect(x + 16, top + 39, gradient_width, 42, f'url(#{identifier})'))
            mid = x + 16 + gradient_width / 2
            body.append(f'<path d="M {mid:g} {top+34:g} v -2 m -5 0 h 10 M {mid:g} {top+86:g} v 5 m -5 0 h 10" stroke="{INK}" stroke-width="2" fill="none"/>')
            result = mix(pair, space, .5)
            body.append(rect(x + panel_width - 76, top + 39, 60, 42, color(result)))
            values = ', '.join(str(v) for v in rgb8(result))
            body.append(text(x + panel_width - 16, top + 107, f'RGB {values}', 17, weight=700, anchor='end'))
            body.append('</g>')
    footer = ['Calculated interpolation, displayed as 8-bit sRGB.',
              'Linear RGB is the additive-light mixture.',
              'This is not a quality ranking; out-of-gamut values are clipped.']
    if mobile:
        footer[-1:] = ['Not a quality ranking.', 'Out-of-gamut values are clipped.']
    body.extend(text(20, height - (len(footer) - i) * 23 + 8, v,
                     14 if mobile else 18, MUTED) for i, v in enumerate(footer))
    return document(width, height, title,
                    'Two calculated endpoint pairs at equal weight. Black and white give '
                    'sRGB 128, linear-light 188, and OKLab interpolation 99 in each byte '
                    'channel. Blue and red show that hue and saturation can also differ. '
                    'All three rows are shown simultaneously, with no interaction required.', body)


def routes(mobile):
    width, height = (380, 1400) if mobile else (960, 900)
    body = [text(20, 35, 'Convert when the operation needs it',
                 20 if mobile else 28, weight=700),
            text(20, 65, 'Four routes through the same color system',
                 15 if mobile else 18, MUTED),
            f'<defs><marker id="arrow" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="7" markerHeight="7" orient="auto"><path d="M 1 1 L 7 4 L 1 7" fill="none" stroke="{MUTED}" stroke-width="1.3"/></marker></defs>']
    top = 98
    for index, route in enumerate(ROUTES):
        y = top if mobile else 106 + index * 185
        body.append(f'<g data-role="conversion-route" data-case="{index}">')
        body.append(text(20, y, route['title'], 18 if mobile else 21, weight=700))
        condition = route['condition']
        if mobile:
            conditions = [
                ['No CMS, mixing or resize; gamma work'],
                ['Linear composition / default resize', 'Gamma work after the operation'],
                ['OKLab palette processing', 'Even without mixing'],
                ['ICC source → linear sRGB', 'Conceptual profile-normalization route'],
            ][index]
            body.extend(text(20, y + 24 + i * 20, v, 14, MUTED) for i, v in enumerate(conditions))
        else:
            body.append(text(20, y + 26, condition, 18, MUTED))
        count = len(route['nodes'])
        points = [(42, y + 77 + i * 57) for i in range(count)] if mobile else [(90 + i * 780 / (count - 1), y + 85) for i in range(count)]
        for i, ((x, cy), name) in enumerate(zip(points, route['nodes'])):
            hub = name in ('linear RGB', 'PCS · D50')
            body.append(f'<circle cx="{x:g}" cy="{cy:g}" r="16" fill="{"#dfedf4" if hub else "#edf0f3"}"/>')
            body.append(f'<circle cx="{x:g}" cy="{cy:g}" r="5" fill="{BLUE if hub else MUTED}"/>')
            body.append(text(76 if mobile else x, cy + 6 if mobile else cy - 26,
                             name, 18, BLUE if hub else INK, 700,
                             'start' if mobile else 'middle'))
            if i + 1 == count:
                continue
            nx, ny = points[i + 1]
            x1, y1 = (x, cy + 18) if mobile else (x + 20, cy)
            x2, y2 = (nx, ny - 19) if mobile else (nx - 22, ny)
            body.append(f'<path d="M {x1:g} {y1:g} L {x2:g} {y2:g}" stroke="{MUTED}" stroke-width="1.5" marker-end="url(#arrow)"/>')
            body.append(text(76 if mobile else (x + nx) / 2,
                             cy + 34 if mobile else cy + 32,
                             route['edges'][i], 14 if mobile else 17, MUTED,
                             anchor='start' if mobile else 'middle'))
        if not mobile:
            body.append(text(20, y + 148, route['note'], 17, MUTED))
        body.append('</g>')
        top += 118 + 57 * (count - 1) + 25
    footer = ['Schematic routes, not buffer allocations or timing measurements.',
              'PCS: XYZ or Lab at D50. Linear RGB: the internal sRGB basis.']
    if mobile:
        footer = ['Schematic routes; not buffer allocations.',
                  'PCS connects profiles using XYZ or Lab.',
                  'XYZ matrices can be combined on the OKLab route.',
                  'A linear loader result skips sRGB decoding.']
    body.extend(text(20, height - (len(footer) - i) * 23 + 4, v,
                     14 if mobile else 17, MUTED) for i, v in enumerate(footer))
    return document(width, height, 'Operation-dependent color conversion',
                    'Four schematic routes show no-op gamma processing, linear-light '
                    'mixing or resize, OKLab conversion via the combined XYZ-to-LMS '
                    'matrices, and an ICC PCS D50 connection adapted toward D65 linear '
                    'sRGB. Nodes do not imply separate image buffers. Loader composition '
                    'is path-dependent; these are not all loader implementations.', body)


def chromaticity_plot(size):
    # Equal data-axis scale is essential: this is an xy diagram, not a sketch.
    import matplotlib as mpl
    from matplotlib.figure import Figure
    from matplotlib.patches import Polygon
    font_size = 14 if size < 400 else 17
    with mpl.rc_context({'svg.fonttype': 'none', 'svg.hashsalt': 'libsixel-colorspace',
                         'font.family': 'sans-serif',
                         'font.sans-serif': ['DejaVu Sans', 'Arial'],
                         'font.size': font_size,
                         'text.color': INK, 'axes.labelcolor': INK,
                         'xtick.color': INK, 'ytick.color': INK}):
        fig = Figure(figsize=(size / 72, size / 72))
        ax = fig.add_axes([.17, .16, .78, .79])
        for name, tone, style in [('Display P3', GOLD, '--'), ('sRGB', BLUE, '-')]:
            points = PRIMARIES[name]
            ax.add_patch(Polygon(points, facecolor=tone, alpha=.06, edgecolor='none'))
            ax.add_patch(Polygon(points, fill=False, edgecolor=tone, lw=1.7, ls=style))
            ax.scatter(*zip(*points), c=tone, s=20, zorder=4)
        labels = [('G · P3', PRIMARIES['Display P3'][1], (7, 7), 'left'),
                  ('G · sRGB', PRIMARIES['sRGB'][1], (7, -2), 'left'),
                  ('R · P3', PRIMARIES['Display P3'][0], (-5, -17), 'right'),
                  ('R · sRGB', PRIMARIES['sRGB'][0], (-5, 12), 'right'),
                  ('B · shared', PRIMARIES['sRGB'][2], (7, -2), 'left')]
        for label, point, offset, align in labels:
            ax.annotate(label, point, xytext=offset, textcoords='offset points',
                        ha=align, va='center', fontsize=font_size)
        for name, marker, offset, align in [('D65', 'o', (-12, -30), 'center'),
                                           ('D50', 'D', (21, 26), 'left')]:
            point = WHITES[name]
            ax.scatter(*point, marker=marker, s=26, edgecolors=INK,
                       facecolors=INK if name == 'D65' else 'white', zorder=5)
            ax.annotate(name, point, xytext=offset, textcoords='offset points',
                        ha=align, va='center', arrowprops={'arrowstyle': '-',
                                                        'color': MUTED, 'lw': .8})
        ax.set(xlim=(.05, .77), ylim=(0, .78), xlabel='x chromaticity',
               ylabel='y chromaticity', xticks=[.2, .4, .6], yticks=[.2, .4, .6])
        ax.set_aspect('equal', adjustable='box')
        ax.grid(color='#dce3e8', lw=.5)
        ax.tick_params(length=0, pad=6)
        for spine in ax.spines.values():
            spine.set_color('#dce3e8')
        buffer = io.StringIO()
        fig.savefig(buffer, format='svg', metadata={'Date': None,
                    'Creator': 'libsixel color-space figure generator'})
        svg = ET.fromstring(buffer.getvalue())
        svg.set('width', str(size))
        svg.set('height', str(size))
        ET.register_namespace('', NS)
        return svg


def primaries(mobile):
    width, height = (380, 820) if mobile else (960, 575)
    body = [text(20, 36, 'Different primaries. The same white.',
                 20 if mobile else 28, weight=700),
            text(20, 66, 'sRGB and Display P3 in CIE 1931 xy',
                 16 if mobile else 18, MUTED)]
    size = 360 if mobile else 448
    plot = chromaticity_plot(size)
    plot.set('x', '0' if mobile else '12')
    plot.set('y', '77')
    body.append(ET.tostring(plot, encoding='unicode'))
    x, y = (24, 478) if mobile else (505, 130)
    notes = [
        ('1  Primaries define the RGB basis',
         ['Triangles join the red, green and blue', 'primary chromaticities. P3 extends farther.']),
        ('2  D65 is the shared white point',
         ['Both spaces use D65 and the sRGB', 'transfer function, but their primaries differ.']),
        ('3  D50 is the ICC PCS reference',
         ['Profile conversion accounts for the', 'D50 / D65 change through color adaptation.']),
    ]
    for i, (heading, values) in enumerate(notes):
        cy = y + i * (94 if mobile else 113)
        body.append(text(x, cy, heading, 17 if mobile else 21, weight=700))
        body.extend(text(x, cy + 28 + j * 23, line, 15 if mobile else 18, MUTED)
                    for j, line in enumerate(values))
    footer = ['Axes are dimensionless; equal x/y scale.',
              'Triangle fills identify spaces, not actual displayed colors.']
    if mobile:
        footer[-1:] = ['Triangle fills identify spaces;', 'they are not actual displayed colors.']
    body.extend(text(20, height - (len(footer) - i) * 22 + 3, line,
                     14 if mobile else 17, MUTED) for i, line in enumerate(footer))
    return document(width, height, 'RGB primaries and reference whites',
                    'CIE 1931 xy chromaticities from CSS Color 4, drawn at equal x/y '
                    'scale. Solid blue is sRGB; dashed gold is Display P3. Both share '
                    'D65 at (0.3127, 0.3290). D50 at (0.3457, 0.3585) is the ICC '
                    'PCS reference. Triangle fill colors are categorical labels only.', body)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    examples = {pair: {space: rgb8(mix(pair, space, .5)) for space in SPACES}
                for pair in PAIRS}
    if [examples['black-white'][s][0] for s in SPACES] != [128, 188, 99]:
        raise SystemExit('The documented neutral midpoint examples changed')
    assets = {f'{name}-{layout}.svg': draw(layout == 'mobile')
              for name, draw in [('mixing', mixing), ('conversion-routes', routes),
                                 ('primaries-whitepoints', primaries)]
              for layout in ('wide', 'mobile')}
    assets['figures.json'] = json.dumps({
        'generator': 'tools/plot_colorspace_figures.py',
        'plot_dependency': 'matplotlib==3.10.3',
        'kinds': {'mixing': 'calculated coordinate interpolation, not a quality ranking',
                  'conversion-routes': 'schematic operation boundaries, not buffer allocations',
                  'primaries-whitepoints': 'standard xy coordinates with equal axis scale'},
        'examples_at_half_weight_rgb8': examples,
        'primaries_xy': PRIMARIES, 'whitepoints_xy': WHITES,
        'routes': ROUTES,
        'sources': ['src/colorspace.c', 'src/planner.c', 'src/frompng.c',
                    'src/icc-apply.c', 'https://www.w3.org/TR/css-color-4/',
                    'https://bottosson.github.io/posts/oklab/',
                    'https://www.color.org/specifications/ICC.1-2022-05.pdf'],
        'layouts': ['wide', 'mobile'],
        'color_roles': {'blue': 'conversion hub or sRGB outline',
                        'gold': 'Display P3 outline',
                        'mixing_swatches': 'calculated sRGB output colors'},
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
