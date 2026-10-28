#!/usr/bin/env python3
"""Validate and plot the archived range-adaptation measurements.

This is a documentation generator, not a regression test or a fresh benchmark.
CSV values are paired decoded-SIXEL observations. Schematic figures are labeled
separately; no display gain is applied to any metric input.
"""
import argparse
import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'docs/encoder/palette-construction/pipeline/range-measurements'
MODELS = ['heckbert', 'kmeans', 'medoids', 'center']
NAMES = {'heckbert': 'Heckbert', 'kmeans': 'k-means', 'medoids': 'k-medoids', 'center': 'k-center'}
METRICS = ['scielab', 'msssim', 'local_mean', 'local_max']
LABELS = ['S-CIELAB (32 ppd)', 'MS-SSIM', 'Local mean ΔE00', 'Local max ΔE00']
COLORS = ['#167e9b', '#c56b18', '#7256a3', '#33834b']
plt.rcParams.update({'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False,
                     'svg.hashsalt': 'libsixel-range-adaptation', 'font.family': 'DejaVu Sans'})


def read_csv(path):
    with path.open() as f:
        return list(csv.DictReader(f))


def delta(row, metric):
    return float(row['adaptive_' + metric]) - float(row['baseline_' + metric])


def key(row):
    return tuple(row[k] for k in ['id', 'model', 'space', 'p'])


def save(fig, out, name):
    fig.savefig(out / name, dpi=160, bbox_inches='tight', metadata={'Date': None} if name.endswith('.svg') else {})
    if name.endswith('.svg'):
        path = out / name
        path.write_text('\n'.join(line.rstrip() for line in path.read_text().splitlines()) + '\n')
        fig.savefig(out / name.replace('.svg', '.png'), dpi=160, bbox_inches='tight')
    plt.close(fig)


def write_csv(path, rows):
    with path.open('w') as f:
        writer = csv.DictWriter(f, list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=DEFAULT)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    data, out = args.data, args.output or args.data / 'figures'
    out.mkdir(parents=True, exist_ok=True)
    rows = read_csv(data / 'quality.csv')
    times = read_csv(data / 'timing.csv')
    assert len(rows) == len({key(r) for r in rows}) == 5634
    assert len(times) == 900
    for r in rows:
        for m in METRICS:
            assert np.isfinite(float(r['baseline_' + m]))
            assert np.isfinite(float(r['adaptive_' + m]))
        assert int(r['changed']) == (r['baseline_rgb_sha256'] != r['adaptive_rgb_sha256'])
        if r['active'] == '1':
            assert int(r['hard']) < int(r['p']) and int(r['safe']) > int(r['hard'])
            assert int(r['candidates']) <= int(r['budget'])
    inputs = {r['id']: r for r in json.loads((data / 'inputs.json').read_text())}
    examples = json.loads((data / 'examples.json').read_text())
    by_key = {key(r): r for r in rows}
    for e in examples:
        row = by_key[(e['id'], e['model'], e['space'], str(e['p']))]
        for arm, path in e['files'].items():
            rgb = Image.open(data / path).convert('RGB').tobytes()
            expected = inputs[e['id']]['input_rgb_sha256'] if arm == 'source' else row[arm + '_rgb_sha256']
            assert hashlib.sha256(rgb).hexdigest() == expected, path

    # The x coordinate remains the original color value. Only bucket addresses
    # are subdivided; the sketch deliberately does not pretend to be a real LUT.
    fig, ax = plt.subplots(3, 1, figsize=(10, 6.6), constrained_layout=True)
    x = np.arange(256)
    for mean, width, title, a in [(115, 42, 'Before a fade: colors span many nominal intervals', ax[0]),
                                 (14, 5, 'Near black: source distinctions crowd into a few intervals', ax[1])]:
        y = np.exp(-((x - mean) / width) ** 2)
        y += .13 * np.exp(-((x - 238) / 3) ** 2)
        a.fill_between(x, y, color=COLORS[0], alpha=.6)
        for edge in range(0, 257, 8):
            a.axvline(edge, color='#999999', lw=.5, alpha=.65)
        a.set(xlim=(0, 255), ylim=(0, 1.2), yticks=[], title=title, ylabel='Sample mass')
    ax[1].annotate('Sparse bright controls remain separate', xy=(238, .13), xytext=(118, .8),
                   arrowprops={'arrowstyle': '->'}, fontsize=10)
    a = ax[2]
    for lo, hi in [(0, 8), (8, 16), (16, 24), (24, 32), (232, 240), (240, 248)]:
        a.axvspan(lo, hi, .12, .6, color=COLORS[0], alpha=.3)
    for edge in range(0, 257, 8):
        a.axvline(edge, color='#999999', lw=.5)
    for edge in [3, 5, 10, 13, 18, 20, 23, 26, 28, 31]:
        a.plot([edge, edge], [.12, .6], color=COLORS[1], lw=1.8)
    a.text(68, .37, 'Empty intervals donate addresses;\noccupied intervals keep their boundaries.', va='center')
    a.set(xlim=(0, 255), ylim=(0, 1), yticks=[], xlabel='Original gamma-RGB channel value (schematic)',
          title='Refinement separates recoverable tones within occupied intervals')
    fig.suptitle('Fade-in / fade-out: population moves; the nominal grid does not\nSchematic distributions and subdivisions, not measured samples', fontsize=13)
    save(fig, out, 'mechanism.svg')

    import re
    source = (ROOT / 'src/palette-common-snap.h').read_text()
    body = source.split('sixel_safe_tones[256] = {', 1)[1].split('};', 1)[0]
    tones = np.array([int(v) for v in re.findall(r'\d+', body)])
    assert len(tones) == 256
    fig, a = plt.subplots(figsize=(9, 4), constrained_layout=True)
    a.step(np.arange(33), tones[:33], where='mid', color=COLORS[0], label='SIXEL safe-tone table')
    a.plot([0, 32], [0, 32], '--', color='#999999', label='Identity (reference)')
    for edge in [8, 16, 24]:
        a.axvline(edge, color=COLORS[1], alpha=.65, label='Nominal 5-bit boundary' if edge == 8 else None)
    a.set(xlim=(0, 32), ylim=(0, 33), xlabel='Original gamma-RGB byte value', ylabel='Safe-tone byte value',
          title='Different bytes can still describe the same output tone')
    a.legend(loc='upper left')
    save(fig, out, 'safe-tones.svg')

    # Pixel-preserving comparisons; the second row is a common encoded-RGB gain,
    # explicitly diagnostic. It is not a new encode and is never scored.
    e = examples[0]
    fig, axes = plt.subplots(2, 3, figsize=(12, 5.5), constrained_layout=True)
    for col, arm in enumerate(['source', 'baseline', 'adaptive']):
        im = np.array(Image.open(data / e['files'][arm]).convert('RGB'))
        for row, gain in enumerate([1, 3]):
            axes[row, col].imshow(np.minimum(im.astype(float) * gain, 255).astype('uint8'), interpolation='nearest')
            axes[row, col].axis('off')
            axes[row, col].set_title(f'{arm.capitalize()} · ' + ('original brightness' if gain == 1 else '×3 RGB inspection gain'))
    fig.suptitle('Sintel trailer, 2.00 s + bright controls · Heckbert · gamma RGB · 256 colors\nDecoded SIXEL; equal display gain on the lower row only', fontsize=12)
    save(fig, out, 'fade-comparison.png')

    v = sorted([r for r in rows if r['id'].endswith('ui1') and r['model']=='heckbert' and r['space']=='gamma' and r['p']=='256'], key=lambda r:r['id'])
    seconds = np.arange(len(v)) * .25
    fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
    a = axes[0, 0]
    for field, label, color in [('hard','Nominal occupied bins H',COLORS[1]),('candidates','Adaptive candidates C',COLORS[0]),('baseline_colors','Used output colors: baseline','#a1673d'),('adaptive_colors','Used output colors: adaptive',COLORS[2])]:
        a.plot(seconds, [int(r[field]) for r in v], label=label, color=color)
    a.axhline(256, color='#888888', ls=':', label='Requested colors P')
    a.set_title('Bins, candidates and used output colors differ');a.set_ylabel('Count');a.legend(fontsize=8)
    for a, m, label in zip(axes.flat[1:], ['scielab','msssim','local_max'], [LABELS[0],LABELS[1],LABELS[3]]):
        for arm, color in [('baseline',COLORS[1]),('adaptive',COLORS[0])]:
            a.plot(seconds, [float(r[arm+'_'+m]) for r in v], label=arm, color=color)
        a.set_ylabel(label);a.set_title('Higher is better' if m=='msssim' else 'Lower is better');a.legend()
    for a in axes.flat: a.set_xlabel('Frame timestamp (s)');a.grid(axis='y', alpha=.18)
    fig.suptitle('Measured opening fade · Heckbert / gamma RGB / P256 / bright controls', fontsize=13)
    save(fig, out, 'video-timeline.svg')

    fig, axes = plt.subplots(4, 2, figsize=(11, 10), constrained_layout=True)
    summary=[]
    for col, video in enumerate([False,True]):
        group=[r for r in rows if r['space']=='gamma' and r['id'].startswith('video')==video]
        for j,m in enumerate(METRICS):
            a=axes[j,col]
            for k,model in enumerate(MODELS):
                means=[]
                for p in ['16','64','256']:
                    sub=[r for r in group if r['model']==model and r['p']==p]
                    means.append(np.mean([delta(r,m) for r in sub]))
                a.plot([0,1,2],means,'o-',color=COLORS[k],label=NAMES[model])
            a.axhline(0,color='#777777',lw=.7);a.set_xticks([0,1,2],['16','64','256'])
            a.set_ylabel('Δ '+LABELS[j]);a.set_xlabel('Requested colors (-p)')
            a.set_title(('400 ordinary inputs' if not video else '32 video inputs')+(' · higher is better' if m=='msssim' else ' · lower is better'), fontsize=10)
            if j==0:a.legend(fontsize=8,ncol=2)
        for model in MODELS:
            sub=[r for r in group if r['model']==model]
            summary.append({'cohort':'video' if video else 'ordinary','model':model,'n':len(sub),'active':sum(int(r['active']) for r in sub),'changed':sum(int(r['changed']) for r in sub),**{m:float(np.mean([delta(r,m) for r in sub])) for m in METRICS}})
    fig.suptitle('Mean paired changes: adaptive minus baseline · gamma RGB\nUnchanged cases included; panels have independent vertical scales',fontsize=13)
    save(fig,out,'quality-by-palette.svg')

    fig,axes=plt.subplots(2,2,figsize=(11,7),constrained_layout=True)
    categories=['photo','cel','chart','flat']
    for a,m,label in zip(axes.flat,METRICS,LABELS):
        for i,cat in enumerate(categories):
            vals=np.array([delta(r,m) for r in rows if r['space']=='gamma' and r['category']==cat])
            # Deterministic spread of individual conditions avoids hiding tails
            # behind the many exact zero changes. Red diamonds are means.
            offsets=.2*np.sin(np.arange(len(vals))*2.399963)
            a.scatter(i+offsets,vals,s=7,alpha=.25,color=COLORS[i],rasterized=True)
            a.scatter([i],[vals.mean()],marker='D',s=42,color='#b22333',zorder=3)
        a.axhline(0,color='#666666',lw=.7);a.set_xticks(range(4),categories);a.set_ylabel('Δ '+label)
        a.set_title('Higher is better' if m=='msssim' else 'Lower is better')
    fig.suptitle('Ordinary-image tails · 1,200 paired conditions per category\nAll 4 models and P16/64/256; diamonds = means; no combined quality score',fontsize=13)
    save(fig,out,'category-tails.svg')

    groups=defaultdict(dict)
    for r in times:
        groups[(key(r),r['round'])][r['arm']]=r
    paired=defaultdict(list)
    for (k,round_id),arms in groups.items():
        assert set(arms)=={'baseline','adaptive'}
        assert all(int(r['repeats'])==5 and int(r['warmups'])==1 for r in arms.values())
        paired[k].append(float(arms['adaptive']['median_ms'])-float(arms['baseline']['median_ms']))
    timing_summary=[]
    for k,ds in sorted(paired.items()):
        assert len(ds)==3
        ref=next(r for r in times if key(r)==k)
        timing_summary.append(dict(zip(['id','model','space','p'],k))|{'active':int(ref['active']),'paired_median_ms':float(np.median(ds)),'round_min_delta_ms':min(ds),'round_max_delta_ms':max(ds)})
    fig,axes=plt.subplots(1,3,figsize=(12,5),constrained_layout=True)
    for ax,space,active,title in [(axes[0],'gamma',0,'RGB: adaptation inactive'),(axes[1],'gamma',1,'RGB: adaptation active'),(axes[2],'lab',1,'Lab family: adaptation active')]:
        for i,model in enumerate(MODELS):
            vals=[r['paired_median_ms'] for r in timing_summary if r['model']==model and r['active']==active and ((r['space']=='gamma')==(space=='gamma'))]
            if not vals:continue
            ax.scatter(vals,i+.12*np.sin(np.arange(len(vals))*2.399963),color=COLORS[i],s=18,alpha=.65)
            ax.scatter([np.median(vals)],[i],color='black',marker='D',s=34,zorder=3)
            ax.text(.97,i+.22,f'n={len(vals)}',transform=ax.get_yaxis_transform(),ha='right',fontsize=8)
        ax.axvline(0,color='#888888',lw=.8);ax.set_yticks(range(4),[NAMES[m] for m in MODELS]);ax.invert_yaxis()
        ax.set_xlabel('Paired median Δ encode time (ms)');ax.set_title(title,fontsize=11)
    fig.suptitle('Timing is not free, even when adaptation stays inactive\nOne dot per selected condition; black diamonds = group medians; independent x scales',fontsize=13)
    save(fig,out,'timing.svg')

    fig,axes=plt.subplots(2,3,figsize=(12,7),constrained_layout=True)
    for row,e in enumerate(examples[2:]):
        for col,arm in enumerate(['source','baseline','adaptive']):
            axes[row,col].imshow(Image.open(data/e['files'][arm]),interpolation='nearest');axes[row,col].axis('off')
            axes[row,col].set_title(f"{e['id']} · {NAMES[e['model']]} · {e['space']}\n{arm.capitalize()} · P{e['p']}",fontsize=10)
    fig.suptitle('Local regressions and large improvements both need inspection\nOriginal brightness; exact source/output PNGs are provided separately',fontsize=13)
    save(fig,out,'ordinary-examples.png')

    top=[]
    for metric in METRICS:
        for direction in ['improved','degraded']:
            sign=1 if metric=='msssim' else -1
            ranked=sorted(rows,key=lambda r:sign*delta(r,metric),reverse=direction=='improved')[:5]
            for rank,r in enumerate(ranked,1):
                top.append({'metric':metric,'direction':direction,'rank':rank,**{k:r[k] for k in ['id','model','space','p']},**{'delta_'+m:delta(r,m) for m in METRICS}})
    write_csv(out/'top-k.csv',top)
    write_csv(out/'quality-summary.csv',summary)
    write_csv(out/'timing-pairs.csv',timing_summary)
    print(f'Validated {len(rows)} quality pairs, {len(times)} timing observations and all 12 image hashes.')
    print(f'Wrote 8 figures and 3 derived tables to {out}')


if __name__ == '__main__':
    main()
