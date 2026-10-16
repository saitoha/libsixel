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
            if axis.get_scale() == 'log':
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
    for image, rows in groups.items():
        corpus = image == 'photo'
        scale_rows = [r for r in rows if r['experiment'] == 'scaling']
        gen_rows = [r for r in rows if r['experiment'] == 'generation']
        codecs = sorted({r['codec'] for r in scale_rows}, key=order)
        heading = (f'Photographs · {len(photos)} images fitted inside {meta["fit"][0]} × {meta["fit"][1]}' if corpus
                   else 'Diagram · 768 × 512')
        for mobile in [False, True]:
            suffix = 'mobile' if mobile else 'wide'
            for chart in ['profile', 'scaling', 'generations']:
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
