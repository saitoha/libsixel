#!/usr/bin/env python3
"""Render saved format measurements without rerunning any timed work."""
import argparse
import json
import io
from pathlib import Path
import statistics
import textwrap

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator, FuncFormatter, NullFormatter
import numpy as np

COLORS = {'SIXEL': '#0072b2', 'GIF': '#b36b00', 'PNG': '#009e73', 'JPEG': '#a84073', 'WebP': '#6457a6'}
MARKERS = {'SIXEL': 'o', 'GIF': 's', 'PNG': 'D', 'JPEG': '^', 'WebP': 'P'}
REP = ['SIXEL default', 'SIXEL none', 'GIF', 'PNG', 'JPEG 80', 'WebP 80', 'WebP lossless']
plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 11,
    'axes.spines.top': False, 'axes.spines.right': False,
    'svg.hashsalt': 'libsixel-format-comparison', 'svg.fonttype': 'none'})


def color(codec):
    return COLORS[codec.split()[0]]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    parser.add_argument('--preview-directory', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.preview_directory:
        args.preview_directory.mkdir(parents=True, exist_ok=True)
    p = json.loads((args.directory / 'results.json').read_text())
    rows = p['rows']
    for image in ['photo', 'gradient', 'diagram']:
        for mobile in [False, True]:
            suffix = 'mobile' if mobile else 'wide'
            for chart in ['profile', 'scaling', 'generations']:
                count = 4
                fig, axes = plt.subplots(count if mobile else 2, 1 if mobile else 2,
                    figsize=(6.2, 17) if mobile else (12, 9), layout='constrained')
                axes = axes.flatten()
                if chart == 'profile':
                    data = [r for r in rows if r['image'] == image and r['experiment'] == 'scaling' and r['threads'] == 1]
                    data.sort(key=lambda r: (list(COLORS).index(r['codec'].split()[0]), r['codec']))
                    for ax, metric, label in zip(axes[:2], ['encode_ms', 'decode_ms'], ['Encode to bytes (ms)', 'Decode to RGB (ms)']):
                        y = np.arange(len(data))
                        med = [statistics.median(r[metric]) for r in data]
                        lo = [m - np.percentile(r[metric], 25) for r, m in zip(data, med)]
                        hi = [np.percentile(r[metric], 75) - m for r, m in zip(data, med)]
                        ax.scatter(med, y, color=[color(r['codec']) for r in data], zorder=3)
                        ax.errorbar(med, y, xerr=[lo, hi], fmt='none', ecolor='#202020', capsize=2)
                        ax.set_yticks(y, [f"{i+1}. {r['codec']}" for i, r in enumerate(data)])
                        ax.invert_yaxis()
                        ax.set_xscale('log')
                        ax.set_xlabel(label + '; median / IQR, log axis')
                    ax = axes[2]
                    for index, r in enumerate(data):
                        ax.scatter(r['transport_bytes'] / 1024, r['ms_ssim'], color=color(r['codec']), marker='o' if r['codec'].startswith('SIXEL') else 's')
                        ax.annotate(str(index + 1), (r['transport_bytes'] / 1024, r['ms_ssim']), xytext=(4, 7 if index % 2 else -11), textcoords='offset points', fontsize=9)
                    ax.set(xscale='log', xlabel='7-bit transport payload (KiB, log axis)', ylabel='MS-SSIM versus source; numbers match top panels')
                    ax.margins(x=.35, y=.2)
                    ax = axes[3]
                    data = [next(r for r in data if r['codec'] == c) for c in REP]
                    y = np.arange(len(data))
                    ax.barh(y, [r['transport_bytes']/1024 for r in data], color='#c4c9cf', label='Base64 payload (binary formats)')
                    ax.barh(y, [r['bytes']/1024 for r in data], color=[color(r['codec']) for r in data], label='Native bytes')
                    ax.set_yticks(y, [r['codec'] for r in data])
                    ax.invert_yaxis()
                    ax.set_xlabel('KiB; SIXEL already includes DCS / ST')
                    ax.legend(fontsize=8, loc='lower right')
                    title = 'Quality, latency and transport cost'
                    note = 'One thread; complete memory adapters; 7 samples after 2 warm-ups. Quality settings are not matched.'
                elif chart == 'scaling':
                    for codec in REP:
                        data = sorted([r for r in rows if r['image'] == image and r['experiment'] == 'scaling' and r['codec'] == codec], key=lambda r: r['threads'])
                        x = [r['threads'] for r in data]
                        for ax, metric in zip(axes[:2], ['encode_ms', 'decode_ms']):
                            y = [statistics.median(r[metric]) for r in data]
                            ax.plot(x, y, marker=MARKERS[codec.split()[0]], label=codec, color=color(codec), linestyle='--' if codec.endswith(('none', 'lossless')) else '-')
                            ax.fill_between(x, [np.percentile(r[metric],25) for r in data], [np.percentile(r[metric],75) for r in data], color=color(codec), alpha=.1)
                        for ax, metric in zip(axes[2:], ['ms_ssim', 'bytes']):
                            ax.plot(x, [r[metric] / (1024 if metric == 'bytes' else 1) for r in data], marker=MARKERS[codec.split()[0]], label=codec, color=color(codec), linestyle='--' if codec.endswith(('none', 'lossless')) else '-')
                    for ax, label in zip(axes, ['Encode (ms, log axis)', 'Decode fixed one-thread stream (ms, log axis)', 'MS-SSIM of each encoder output', 'Each encoder output (KiB)']):
                        ax.set_xticks([1,2,4,8])
                        ax.set(xlabel='Configured budget (not reserved CPU cores)', ylabel=label)
                    for ax in axes[:2]:
                        ax.set_yscale('log')
                    fig.legend(*axes[0].get_legend_handles_labels(), loc='outside lower center', ncol=3 if not mobile else 2, fontsize=8)
                    title = 'A thread budget does not guarantee linear scaling'
                    note = 'SIXEL: N-worker budget. WebP: parallel switch at N > 1. Pillow JPEG/GIF/PNG remain serial.'
                else:
                    for codec in ['SIXEL default', 'GIF', 'PNG', 'JPEG 80', 'WebP 80', 'WebP lossless']:
                        for mode in (['indexed'] if codec in ['SIXEL default','GIF'] else ['RGB']):
                            data = sorted([r for r in rows if r['image'] == image and r['experiment'] == 'generation' and r['codec'] == codec and r['mode'] == mode], key=lambda r:r['generation'])
                            x = [r['generation'] for r in data]
                            label = codec
                            for ax, metric in zip(axes, ['ms_ssim','delta_e','encode_ms','decode_ms']):
                                y = [statistics.median(r[metric]) if isinstance(r[metric],list) else r[metric] for r in data]
                                ax.plot(x,y,label=label,color=color(codec),marker='x' if mode=='indexed' else MARKERS[codec.split()[0]],markersize=4,linestyle='--' if mode=='indexed' or codec.endswith('lossless') else '-')
                    for ax,label in zip(axes,['MS-SSIM versus original (higher is better)','Mean Delta E00 vs original (lower is better)','Encode each generation (ms, log axis)','Decode each generation (ms, log axis)']):
                        ax.set(xlabel='Encode / decode generation',ylabel=label)
                        ax.set_xticks(range(1,p['metadata']['generations']+1))
                    for ax in axes[2:]:
                        ax.set_yscale('log')
                    fig.legend(*axes[0].get_legend_handles_labels(), loc='outside lower center', ncol=3 if not mobile else 2, fontsize=8)
                    title = 'Repeated round trips with native decoded representations'
                    note = 'One thread; 7 samples per generation. SIXEL/GIF retain indices and palette; other formats use RGB.'
                for ax in axes:
                    ax.grid(axis='x', alpha=.15)
                    for axis in [ax.xaxis, ax.yaxis]:
                        if axis.get_scale() == 'log':
                            axis.set_major_locator(LogLocator(base=10, subs=(1,2,5)))
                            axis.set_major_formatter(FuncFormatter(lambda x, _: f'{x:g}'))
                            axis.set_minor_formatter(NullFormatter())
                wrapped = textwrap.fill(note, width=64 if mobile else 110)
                fig.suptitle(f'{image.title()} · 768 × 512\n{title}\n{wrapped}', fontsize=10)
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
