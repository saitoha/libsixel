#!/usr/bin/env python3
"""Render saved format measurements without rerunning any timed work.

The photographic workload is a corpus: each photograph contributes the median
of its timed samples, and the charts show the distribution of those
per-photograph values (box plots) or their mean. The diagram workload is one
image and keeps the per-sample median and interquartile range.
"""
import argparse
import io
import json
from pathlib import Path
import statistics
import textwrap

import matplotlib
matplotlib.use('Agg')
import matplotlib.patches
import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator, FuncFormatter, NullFormatter
import numpy as np

COLORS = {'SIXEL': '#0072b2', 'GIF': '#b36b00', 'PNG': '#009e73', 'QOI': '#d55e00',
          'JPEG': '#a84073', 'WebP': '#6457a6', 'AVIF': '#56b4e9', 'HEIC': '#8c7a00'}
MARKERS = {'SIXEL': 'o', 'GIF': 's', 'PNG': 'D', 'QOI': 'v', 'JPEG': '^', 'WebP': 'P', 'AVIF': 'X', 'HEIC': 'h'}
REP = ['SIXEL default', 'SIXEL quality', 'GIF', 'PNG', 'QOI', 'JPEG 80', 'WebP 80', 'WebP lossless', 'AVIF 80', 'HEIC 80']
GENERATIONS = ['SIXEL default', 'GIF', 'PNG', 'QOI', 'JPEG 80', 'WebP 80', 'WebP lossless', 'AVIF 80', 'HEIC 80']
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 11,
    'axes.spines.top': False, 'axes.spines.right': False,
    'svg.hashsalt': 'libsixel-format-comparison', 'svg.fonttype': 'none'})


def color(codec):
    return COLORS[codec.split()[0]]


def style(codec):
    if codec.endswith('lossless'):
        return '--'
    if codec.endswith('quality'):
        return '-.'
    return '-'


def order(codec):
    return (list(COLORS).index(codec.split()[0]), codec)


def median(values):
    return statistics.median(values) if isinstance(values, list) else values


def per_image(rows, codec, metric, **match):
    """Return one value per image: the median of its samples."""
    return [median(r[metric]) for r in rows if r['codec'] == codec
            and all(r.get(k) == v for k, v in match.items())]


def log_axes(axes):
    for ax in axes:
        ax.grid(axis='x', alpha=.15)
        for axis in [ax.xaxis, ax.yaxis]:
            # The size sweep labels its x positions with the crop sizes.
            if axis.get_scale() == 'log' and not getattr(axis, 'crop_ticks', False):
                lo, hi = sorted(ax.get_xlim() if axis is ax.xaxis else ax.get_ylim())
                # Crowded 1-2-5 labels overlap on wide ranges.
                subs = (1, 2, 5) if np.log10(hi / lo) <= 2.5 else (1,)
                axis.set_major_locator(LogLocator(base=10, subs=subs))
                axis.set_major_formatter(FuncFormatter(lambda x, _: f'{x:g}'))
                axis.set_minor_formatter(NullFormatter())


def profile(axes, rows, codecs, corpus):
    for ax, metric, label in zip(axes[:2], ['encode_ms', 'decode_ms'],
                                 ['Encode to bytes (ms)', 'Decode to RGB (ms)']):
        y = np.arange(len(codecs))
        if corpus:
            data = [per_image(rows, c, metric, threads=1) for c in codecs]
            box = ax.boxplot(data, positions=y, vert=False, widths=.6, patch_artist=True,
                             showfliers=True, flierprops={'markersize': 2},
                             medianprops={'color': '#202020'})
            for patch, codec in zip(box['boxes'], codecs):
                patch.set(facecolor=color(codec), alpha=.35, edgecolor=color(codec))
            suffix = 'per-photo medians, quartile boxes, log axis'
        else:
            samples = [next(r[metric] for r in rows if r['codec'] == c and r['threads'] == 1) for c in codecs]
            med = [statistics.median(s) for s in samples]
            lo = [m - np.percentile(s, 25) for s, m in zip(samples, med)]
            hi = [np.percentile(s, 75) - m for s, m in zip(samples, med)]
            ax.scatter(med, y, color=[color(c) for c in codecs], zorder=3)
            ax.errorbar(med, y, xerr=[lo, hi], fmt='none', ecolor='#202020', capsize=2)
            suffix = 'median / IQR, log axis'
        ax.set_yticks(y, [f'{i + 1}. {c}' for i, c in enumerate(codecs)])
        ax.invert_yaxis()
        ax.set_xscale('log')
        ax.set_xlabel(f'{label}\n{suffix}')
    ax = axes[2]
    for index, codec in enumerate(codecs):
        size = np.array(per_image(rows, codec, 'transport_bytes', threads=1)) / 1024
        score = np.array(per_image(rows, codec, 'ms_ssim', threads=1))
        marker = 'o' if codec.startswith('SIXEL') else 's'
        if corpus:
            ax.scatter(size, score, color=color(codec), marker=marker, s=6, alpha=.18)
        ax.scatter(size.mean(), score.mean(), color=color(codec), marker=marker, s=46,
                   edgecolor='#202020', linewidth=.6, zorder=3)
        ax.annotate(str(index + 1), (size.mean(), score.mean()),
                    xytext=(4, 7 if index % 2 else -11), textcoords='offset points', fontsize=9)
    ylabel = 'MS-SSIM versus source; numbers match top panels'
    if corpus:
        ylabel = 'MS-SSIM; large marks are means, small marks photos'
    ax.set(xscale='log', xlabel='7-bit transport payload (KiB, log axis)', ylabel=ylabel)
    ax.margins(x=.2, y=.1)
    if corpus:
        # A few photographs score far below the rest; keep the bulk readable
        # and state what lies below the axis instead of hiding it.
        scores = [s for c in codecs for s in per_image(rows, c, 'ms_ssim', threads=1)]
        floor = min(0.975, float(np.percentile(scores, 1)))
        below = [(c, s) for c in codecs for s in per_image(rows, c, 'ms_ssim', threads=1) if s < floor]
        ax.set_ylim(floor, 1.0005)
        if below:
            names = sorted({c for c, _ in below})
            ax.text(.02, .02, f'{len(below)} points below {floor:.3f} not shown '
                    f'({", ".join(names)}; lowest {min(s for _, s in below):.3f})',
                    transform=ax.transAxes, fontsize=8, color='#505050')
    ax = axes[3]
    y = np.arange(len(REP))
    native = [np.mean(per_image(rows, c, 'bytes', threads=1)) / 1024 for c in REP]
    transport = [np.mean(per_image(rows, c, 'transport_bytes', threads=1)) / 1024 for c in REP]
    ax.barh(y, transport, color='#c4c9cf', label='Base64 payload (binary formats)')
    ax.barh(y, native, color=[color(c) for c in REP], label='Native bytes')
    ax.set_yticks(y, REP)
    ax.invert_yaxis()
    ax.set_xlabel(('Mean KiB per photo' if corpus else 'KiB') + '; SIXEL already includes DCS / ST')
    ax.set_xlim(0, max(transport) * 1.55)  # leave room for the legend
    ax.legend(fontsize=8, loc='lower right')


def scaling(axes, rows, corpus):
    for codec in REP:
        x = [1, 2, 4, 8]
        for ax, metric in zip(axes[:2], ['encode_ms', 'decode_ms']):
            values = [per_image(rows, codec, metric, threads=t) for t in x]
            if corpus:
                y = [np.mean(v) for v in values]
                lo = [np.percentile(v, 25) for v in values]
                hi = [np.percentile(v, 75) for v in values]
            else:
                samples = [next(r[metric] for r in rows if r['codec'] == codec and r['threads'] == t) for t in x]
                y = [statistics.median(s) for s in samples]
                lo = [np.percentile(s, 25) for s in samples]
                hi = [np.percentile(s, 75) for s in samples]
            ax.plot(x, y, marker=MARKERS[codec.split()[0]], label=codec, color=color(codec), linestyle=style(codec))
            ax.fill_between(x, lo, hi, color=color(codec), alpha=.1)
        for ax, metric in zip(axes[2:], ['ms_ssim', 'bytes']):
            y = [np.mean(per_image(rows, codec, metric, threads=t)) / (1024 if metric == 'bytes' else 1) for t in x]
            ax.plot(x, y, marker=MARKERS[codec.split()[0]], label=codec, color=color(codec), linestyle=style(codec))
    labels = ['Encode (ms, log axis)', 'Decode fixed one-thread stream (ms, log axis)',
              'MS-SSIM of each encoder output', 'Each encoder output (KiB)']
    if corpus:
        labels = ['Encode: mean of per-photo medians (ms, log)', 'Decode: mean of per-photo medians (ms, log)',
                  'Mean MS-SSIM of each encoder output', 'Mean encoder output (KiB)']
    for ax, label in zip(axes, labels):
        ax.set_xticks([1, 2, 4, 8])
        ax.set(xlabel='Configured budget (not reserved CPU cores)', ylabel=label)
    for ax in axes[:2]:
        ax.set_yscale('log')


def generations(axes, rows, count):
    for codec in GENERATIONS:
        mode = 'indexed' if codec in ['SIXEL default', 'GIF'] else 'RGB'
        x = list(range(1, count + 1))
        for ax, metric in zip(axes, ['ms_ssim', 'delta_e', 'encode_ms', 'decode_ms']):
            y = [np.mean(per_image(rows, codec, metric, mode=mode, generation=g)) for g in x]
            ax.plot(x, y, label=codec, color=color(codec), marker='x' if mode == 'indexed' else MARKERS[codec.split()[0]],
                    markersize=4, linestyle='--' if mode == 'indexed' or codec.endswith('lossless') else '-')
    for ax, label in zip(axes, ['MS-SSIM versus original (higher is better)', 'Mean Delta E00 vs original (lower is better)',
                                'Encode each generation (ms, log axis)', 'Decode each generation (ms, log axis)']):
        ax.set(xlabel='Encode / decode generation', ylabel=label)
        ax.set_xticks(range(1, count + 1))
    for ax in axes[2:]:
        ax.set_yscale('log')


def sizes(axes, rows):
    """Plot latency against pixel count for 1:1 center crops of the originals."""
    crops = sorted({tuple(r['size']) for r in rows}, key=lambda c: c[0] * c[1])
    x = [w * h for w, h in crops]
    panels = [('encode_ms', 1), ('encode_ms', 8), ('decode_ms', 1), ('decode_ms', 8)]
    for codec in REP:
        for ax, (metric, threads) in zip(axes, panels):
            values = [per_image(rows, codec, metric, threads=threads, size=list(c)) for c in crops]
            for px, v in zip(x, values):
                ax.scatter([px] * len(v), v, color=color(codec), s=4, alpha=.15, linewidth=0)
            ax.plot(x, [np.mean(v) for v in values], marker=MARKERS[codec.split()[0]], markersize=4,
                    label=codec, color=color(codec), linestyle=style(codec))
    labels = ['Encode, budget 1', 'Encode, budget 8', 'Decode, budget 1', 'Decode, budget 8']
    for ax, label in zip(axes, labels):
        ax.set(xscale='log', yscale='log', ylabel=f'{label} (ms, log axis)',
               xlabel='Pixels per image (log axis)')
        ax.set_xticks(x, [f'{w}×{h}' for w, h in crops])
        ax.xaxis.set_minor_formatter(NullFormatter())
        ax.xaxis.crop_ticks = True


# Stage roles shared across codecs, so equal work has equal color.
STAGE_ROLES = [
    ('color conversion', 'Color conversion', '#999933'),
    ('sampling', 'Sampling (histogram / initialization)', '#88ccee'),
    ('palette search', 'Palette search', '#44aa99'),
    ('lookup setup', 'Lookup setup', '#ddcc77'),
    ('lookup', 'Lookup and dither (GIF: lookup only)', '#cc6677'),
    ('filter', 'PNG row filter', '#aa4499'),
    ('encode', 'Output coding (SIXEL bands, LZW, DEFLATE)', '#332288'),
    ('other', 'Other (copies, container, wrapper)', '#cfcfcf'),
]
STAGE_ROLE = {'color conversion': 'color conversion', 'sampling': 'sampling',
              'palette search': 'palette search', 'lookup setup': 'lookup setup',
              'lookup + dither': 'lookup', 'lookup': 'lookup', 'filter': 'filter',
              'SIXEL encode': 'encode', 'LZW encode': 'encode', 'DEFLATE': 'encode',
              'other': 'other'}


SIZE_NAMES = {(640, 480): 'VGA', (1920, 1080): 'FHD'}


def stage_means(payload, codec, size):
    """Mean over photographs of each stage's per-photograph median."""
    rows = [r for r in payload['rows'] if r['codec'] == codec and r['size'] == size]
    names = payload['metadata']['stages'][codec.split()[0]]
    return {n: np.mean([statistics.median(s['stages'][n] for s in r['samples']) for r in rows])
            for n in names}


def stages_chart(ax, payload, per_pixel):
    meta = payload['metadata']
    sizes = meta['sizes']
    width = .34
    for index, codec in enumerate(meta['codecs']):
        bars = []
        for k, size in enumerate(sizes):
            means = stage_means(payload, codec, size)
            scale = 1e6 / (size[0] * size[1]) if per_pixel else 1
            x = index + (k - .5) * (width + .08)
            bottom, edges = 0.0, []
            for name, value in means.items():
                value *= scale
                role = STAGE_ROLE[name]
                ax.bar(x, value, width, bottom=bottom, color=dict((r, c) for r, _, c in STAGE_ROLES)[role],
                       edgecolor='white', linewidth=.4)
                bottom += value
                edges.append((role, bottom))
            ax.text(x, bottom, f'{bottom:.0f}' if bottom >= 10 else f'{bottom:.1f}',
                    ha='center', va='bottom', fontsize=8)
            bars.append((x, edges))
        # Dotted connectors join the same stage boundary in both sizes.
        (x0, e0), (x1, e1) = bars
        for (role, y0), (_, y1) in zip(e0, e1):
            ax.plot([x0 + width / 2, x1 - width / 2], [y0, y1], linestyle=':', linewidth=1,
                    color=dict((r, c) for r, _, c in STAGE_ROLES)[role])
    ax.set_xticks([i + (k - .5) * (width + .08) for i in range(len(meta['codecs'])) for k in range(len(sizes))],
                  [SIZE_NAMES.get((w, h), f'{w}×{h}') for _ in meta['codecs'] for w, h in sizes], fontsize=8)
    for i, codec in enumerate(meta['codecs']):
        ax.text(i, -.13, codec, transform=ax.get_xaxis_transform(), ha='center', va='top', fontsize=10)
    ax.set_ylabel('Nanoseconds per pixel' if per_pixel else 'Milliseconds per image')
    ax.margins(y=.08)


def render_stages(directory, preview, check):
    payload = json.loads((directory / 'stages.json').read_text())
    meta = payload['metadata']
    handles = [matplotlib.patches.Patch(color=c, label=l) for _, l, c in STAGE_ROLES]
    for mobile in [False, True]:
        suffix = 'mobile' if mobile else 'wide'
        fig, axes = plt.subplots(2 if mobile else 1, 1 if mobile else 2,
                                 figsize=(6.2, 12) if mobile else (13, 6.2), layout='constrained')
        for ax, per_pixel in zip(axes, [False, True]):
            stages_chart(ax, payload, per_pixel)
            ax.set_title('Time per image' if not per_pixel else 'Time per pixel: fixed work shrinks at the larger size',
                         fontsize=10)
        fig.legend(handles=handles, loc='outside lower center', ncol=2 if mobile else 4, fontsize=8)
        note = (f'One worker; mean over {len(meta["images"])} photographs of each stage\'s median of '
                f'{meta["runs"]} samples after 2 warm-ups; 1:1 center crops of the same originals at '
                + ' and '.join(f'{SIZE_NAMES.get(tuple(c), "")} {c[0]} × {c[1]}'.strip() for c in meta['sizes']) + '. '
                'Dotted lines join the same stage boundary at the two sizes.')
        fig.suptitle('Where encoding time goes\n' + textwrap.fill(note, width=64 if mobile else 120), fontsize=10)
        target = directory / f'photo-stages-{suffix}.svg'
        stream = io.BytesIO()
        fig.savefig(stream, format='svg', metadata={'Date': None})
        svg = b'\n'.join(line.rstrip() for line in stream.getvalue().splitlines()) + b'\n'
        if check:
            if target.read_bytes() != svg:
                raise RuntimeError(f'stale figure: {target}')
        else:
            target.write_bytes(svg)
        if preview:
            fig.savefig(preview / f'photo-stages-{suffix}.png', dpi=110)
        plt.close(fig)


# Worker-budget segments: role color, and hatching for work that overlaps
# the color conversion. Groups give the boundaries that dotted lines join.
SEGMENT_STYLE = {
    'color conversion': ('color conversion', None, 0),
    'sampling': ('sampling', None, 0),
    'sampling | conversion': ('sampling', '//', 0),
    'palette search': ('palette search', None, 0),
    'palette search | conversion': ('palette search', '//', 0),
    'lookup setup': ('lookup setup', None, 1),
    'lookup + dither': ('lookup', None, 2),
    'SIXEL encode': ('encode', None, 2),
    'lookup + dither + encode': ('pipeline', None, 2),
    'other': ('other', None, 3),
}
PIPELINE_COLOR = '#7d4f9e'


def render_threads(directory, preview, check):
    payload = json.loads((directory / 'stages.json').read_text())
    meta = payload['metadata']
    colors = dict((r, c) for r, _, c in STAGE_ROLES)
    colors['pipeline'] = PIPELINE_COLOR
    budgets = meta['thread_budgets']
    rows = payload['thread_rows']
    for mobile in [False, True]:
        suffix = 'mobile' if mobile else 'wide'
        fig, ax = plt.subplots(figsize=(6.2, 7) if mobile else (10, 6.2), layout='constrained')
        width = .3
        codecs = [c for c in meta['codecs'] if c.startswith('SIXEL')]
        for index, codec in enumerate(codecs):
            bars = []
            for k, threads in enumerate(budgets):
                group = [r for r in rows if r['codec'] == codec and r['threads'] == threads]
                means = {n: np.mean([statistics.median(s['segments'][n] for s in r['samples']) for r in group])
                         for n in meta['segments']}
                x = index + (k - .5) * (width + .12)
                bottom, tops = 0.0, {}
                for name, value in means.items():
                    role, hatch, level = SEGMENT_STYLE[name]
                    if value > 0:
                        ax.bar(x, value, width, bottom=bottom, color=colors[role], hatch=hatch,
                               edgecolor='white', linewidth=.4)
                    bottom += value
                    tops[level] = bottom
                ax.text(x, bottom, f'{bottom:.0f} ms', ha='center', va='bottom', fontsize=8)
                bars.append((x, tops))
            (x0, t0), (x1, t1) = bars
            for level in sorted(t0):
                ax.plot([x0 + width / 2, x1 - width / 2], [t0[level], t1[level]], linestyle=':',
                        linewidth=1, color='#505050')
        ax.set_xticks([i + (k - .5) * (width + .12) for i in range(len(codecs)) for k in range(len(budgets))],
                      [f'{t} worker' + ('s' if t > 1 else '') for _ in codecs for t in budgets], fontsize=9)
        for i, codec in enumerate(codecs):
            ax.text(i, -.08, codec, transform=ax.get_xaxis_transform(), ha='center', va='top', fontsize=10)
        ax.set_ylabel('Milliseconds per image')
        labels = {'lookup': 'Lookup and dither', 'encode': 'SIXEL band coding',
                  'pipeline': 'Lookup, dither and band coding (pipelined)', 'other': 'Other'}
        handles = [matplotlib.patches.Patch(color=colors[r], label=labels.get(r, l)) for r, l, _ in STAGE_ROLES
                   if r != 'filter' and r != 'other']
        handles += [matplotlib.patches.Patch(color=PIPELINE_COLOR, label=labels['pipeline']),
                    matplotlib.patches.Patch(color=colors['other'], label=labels['other'])]
        handles.append(matplotlib.patches.Patch(facecolor='white', edgecolor='#505050', hatch='//',
                                                label='Runs while the color conversion continues'))
        fig.legend(handles=handles, loc='outside lower center', ncol=2 if mobile else 3, fontsize=8)
        w, h = meta['thread_size']
        note = (f'{w} × {h} crops of {len(meta["images"])} photographs; mean of each segment\'s median over '
                f'{meta["runs"]} samples. Bars partition elapsed time by the work running in each interval; '
                'dotted lines join the ends of palette construction, lookup setup, the dither and coding phase, '
                'and the whole encode.')
        fig.suptitle('Worker budgets shorten some stages and not others\n'
                     + textwrap.fill(note, width=64 if mobile else 110), fontsize=10)
        target = directory / f'photo-threads-{suffix}.svg'
        stream = io.BytesIO()
        fig.savefig(stream, format='svg', metadata={'Date': None})
        svg = b'\n'.join(line.rstrip() for line in stream.getvalue().splitlines()) + b'\n'
        if check:
            if target.read_bytes() != svg:
                raise RuntimeError(f'stale figure: {target}')
        else:
            target.write_bytes(svg)
        if preview:
            fig.savefig(preview / f'photo-threads-{suffix}.png', dpi=110)
        plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--preview-directory', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.preview_directory:
        args.preview_directory.mkdir(parents=True, exist_ok=True)
    p = json.loads((args.directory / 'results.json').read_text())
    meta = p['metadata']
    photos = set(meta['photo_sources'])
    groups = {'photo': [r for r in p['rows'] if r['image'] in photos],
              'diagram': [r for r in p['rows'] if r['image'] == 'diagram']}
    runs = meta['runs']
    if (args.directory / 'stages.json').exists():
        render_stages(args.directory, args.preview_directory, args.check)
        if json.loads((args.directory / 'stages.json').read_text()).get('thread_rows'):
            render_threads(args.directory, args.preview_directory, args.check)
    for image, rows in groups.items():
        corpus = image == 'photo'
        scale_rows = [r for r in rows if r['experiment'] == 'scaling']
        gen_rows = [r for r in rows if r['experiment'] == 'generation']
        size_rows = [r for r in rows if r['experiment'] == 'size']
        codecs = sorted({r['codec'] for r in scale_rows}, key=order)
        photo_sizes = {tuple(meta['input_sizes'][k]) for k in photos}
        if len(photo_sizes) == 1:
            w, h = next(iter(photo_sizes))
            heading = f'Photographs · {len(photos)} images, {w} × {h}'
        else:
            heading = f'Photographs · {len(photos)} images fitted inside {meta["fit"][0]} × {meta["fit"][1]}'
        if not corpus:
            heading = 'Diagram · 768 × 512'
        for mobile in [False, True]:
            suffix = 'mobile' if mobile else 'wide'
            for chart in ['profile', 'scaling', 'generations'] + (['sizes'] if size_rows else []):
                fig, axes = plt.subplots(4 if mobile else 2, 1 if mobile else 2,
                    figsize=(6.2, 17) if mobile else (12, 9), layout='constrained')
                axes = axes.flatten()
                if chart == 'profile':
                    profile(axes, scale_rows, codecs, corpus)
                    title = 'Quality, latency and transport cost'
                    note = f'One thread; complete memory adapters; {runs} samples after 2 warm-ups. Quality settings are not matched.'
                elif chart == 'scaling':
                    scaling(axes, scale_rows, corpus)
                    fig.legend(*axes[0].get_legend_handles_labels(), loc='outside lower center', ncol=2 if mobile else 4, fontsize=8)
                    title = 'A thread budget does not guarantee linear scaling'
                    note = 'SIXEL, AVIF (aom/dav1d) and HEIC (x265/libde265): N workers. WebP: parallel switch at N > 1. Pillow JPEG/GIF/PNG and QOI remain serial.'
                    if corpus:
                        note += ' Bands show the per-photo interquartile range.'
                elif chart == 'sizes':
                    sizes(axes, size_rows)
                    fig.legend(*axes[0].get_legend_handles_labels(), loc='outside lower center', ncol=2 if mobile else 4, fontsize=8)
                    title = 'Latency against image size'
                    sweep = meta['size_sweep']
                    note = (f'1:1 center crops of {len(sweep["images"])} originals (every {len(photos) // len(sweep["images"])}th by name); '
                            f'{runs} samples after 2 warm-ups; decoders read the one-thread stream. Lines are means of per-photo medians; dots are photos.')
                else:
                    generations(axes, gen_rows, meta['generations'])
                    fig.legend(*axes[0].get_legend_handles_labels(), loc='outside lower center', ncol=2 if mobile else 4, fontsize=8)
                    title = 'Repeated round trips with native decoded representations'
                    note = f'One thread; {runs} samples per generation. SIXEL/GIF retain indices and palette; other formats use RGB.'
                    if corpus:
                        count = len(meta['generation_images']) - 1
                        note += f' Lines are means over {count} photographs, every {len(photos) // count}th by name.'
                log_axes(axes)
                wrapped = textwrap.fill(note, width=64 if mobile else 110)
                fig.suptitle(f'{heading}\n{title}\n{wrapped}', fontsize=10)
                target = args.directory / f'{image}-{chart}-{suffix}.svg'
                stream = io.BytesIO()
                fig.savefig(stream, format='svg', metadata={'Date': None})
                svg = b'\n'.join(line.rstrip() for line in stream.getvalue().splitlines()) + b'\n'
                if args.check:
                    if target.read_bytes() != svg:
                        raise RuntimeError(f'stale figure: {target}')
                else:
                    target.write_bytes(svg)
                if args.preview_directory:
                    fig.savefig(args.preview_directory / f'{image}-{chart}-{suffix}.png', dpi=110)
                plt.close(fig)


if __name__ == '__main__':
    main()
