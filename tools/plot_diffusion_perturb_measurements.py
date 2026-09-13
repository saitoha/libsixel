#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reproduce FS perturbation spectra and pixel-exact visual comparisons.

Requires NumPy, Pillow, and Matplotlib. Crop geometry is fixed before encoding.
Intermediate SIXEL, palette, and diagnostic files remain in the output directory.
"""
from pathlib import Path
import argparse
import csv
import datetime
import hashlib
import json
import os
import platform
import subprocess

import numpy as np
from PIL import Image
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

from plot_lookup_policy_speed import program_record, read_build_configuration

ROOT = Path(__file__).resolve().parents[1]
OUT = None
ENV = {k: v for k, v in os.environ.items() if not k.startswith('SIXEL_')}
ENCODER = str(ROOT / 'converters/img2sixel')
DECODER = str(ROOT / 'converters/sixel2png')
COMMON = ['-v', '--threads=1', '--precision=8bit', '--gpu-policy=off',
          '--lookup-policy=none', '-Wgamma', '-Ugamma']
CONDITIONS = [
    ('fs-0', 'FS / P=0', 'raster', 0),
    ('fs-05', 'FS / P=0.5', 'raster', 0.5),
    ('serp-0', 'Serpentine / P=0', 'serpentine', 0),
    ('serp-05', 'Serpentine / P=0.5', 'serpentine', 0.5),
]
MANIFEST = {}


def portable(value):
    """Keep executable and output locations relocatable in published commands."""
    value = str(value)
    for actual, token in ((ENCODER, '{img2sixel}'),
                          (DECODER, '{sixel2png}'),
                          (str(OUT), '{output}'),
                          (str(ROOT), '{source}')):
        value = value.replace(actual, token)
    return value


def configure():
    """Record the build being measured without changing the runtime sources."""
    global OUT, ENCODER, DECODER
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, default=ROOT)
    args = parser.parse_args()
    OUT = args.output_dir.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    build = args.build_dir.resolve()
    ENCODER = str(build / 'converters/img2sixel')
    DECODER = str(build / 'converters/sixel2png')
    # Documentation and renderer edits can coexist with a committed runtime.
    runtime_paths = ['src', 'include', 'converters', 'config.h', 'configure.ac']
    for staged in ([], ['--cached']):
        subprocess.run(['git', 'diff', *staged, '--exit-code', '--quiet',
                        '--', *runtime_paths], cwd=ROOT, check=True)
    MANIFEST.update({
        'schema': 1,
        'revision': subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'measured_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'runtime_source_state': 'clean',
        'platform': platform.platform(),
        'requested_precision': '8bit', 'effective_format': 'rgb888',
        'environment': 'Inherited SIXEL_* variables removed',
        'dependencies': {'numpy': np.__version__,
                         'pillow': Image.__version__,
                         'matplotlib': matplotlib.__version__},
        'build': read_build_configuration(build),
        'encoder': program_record(ENCODER, ROOT),
        'decoder': program_record(DECODER, ROOT),
        'script_sha256': sha(Path(__file__)),
        'commands': [], 'sources': {}, 'outputs': [], 'sheets': [],
    })
    # Hash the linked library as well as the libtool launcher and payload.
    candidates = [build / 'src/.libs/libsixel.1.dylib',
                  build / 'src/.libs/libsixel.so',
                  build / 'src/.libs/libsixel.a']
    MANIFEST['libraries'] = [dict(file=path.relative_to(build).as_posix(),
                                  sha256=sha(path))
                             for path in candidates if path.is_file()]


def spectra(data):
    """Retain the original unwindowed, per-tone RAPSD experiment exactly."""
    size = 256
    frequency = np.fft.fftfreq(size)
    radius = np.hypot(frequency[:, None], frequency[None, :])
    bins = np.floor(radius * size + 0.5).astype(int)
    counts = np.bincount(bins.ravel())
    radial_frequency = np.arange(len(counts)) / size
    rows, curves = [], []
    fig, axes = plt.subplots(2, 2, figsize=(12, 8.5),
                             sharex=True, sharey=True, layout='constrained')
    colors = {'raster': '#17689b', 'serpentine': '#b45916'}
    for g, ax in zip((32, 64, 85, 128), axes.flat):
        for ident, label, scan, amount in CONDITIONS:
            pixels = np.asarray(data[f'g{g}'][ident])[:, :, 0] / 255.0
            power = np.abs(np.fft.fft2(pixels - pixels.mean())) ** 2
            power[0, 0] = 0
            radial = np.bincount(bins.ravel(), weights=power.ravel()) / counts
            # Weight the energy share by original 2-D cells, not radial bins.
            cutoff = np.sqrt(g / 255)
            low = power[radius < cutoff].sum() / power.sum()
            peak = 1 + np.argmax(radial[1:])
            row = dict(g=g, condition=label, mean=float(pixels.mean()),
                       fb=float(cutoff), low_percent=float(100 * low),
                       peak_ratio=float(radial[peak] / radial[1:].mean()),
                       peak_frequency=float(radial_frequency[peak]),
                       bytes=(OUT / f'g{g}-{ident}.six').stat().st_size)
            rows.append(row)
            for index, value in enumerate(radial):
                curves.append(dict(g=g, condition=label,
                                   cycles_per_pixel=float(radial_frequency[index]),
                                   mean_power=float(value),
                                   bin_count=int(counts[index])))
            ax.semilogy(radial_frequency[1:], radial[1:], label=label,
                        color=colors[scan], ls='--' if amount else '-', lw=1.4)
        if cutoff <= radius.max():
            ax.axvline(cutoff, color='#777777', ls=':', lw=1)
        else:
            ax.text(.04, .94, 'Cutoff exceeds grid: energy share = 100%',
                    transform=ax.transAxes, va='top', fontsize=9)
        ax.set(title=f'g = {g} / 255', xlabel='Radial frequency (cycles/pixel)',
               ylabel='Mean squared FFT magnitude')
        ax.set_xlim(0, np.sqrt(.5))
        ax.grid(alpha=.2)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='outside lower center', ncol=4, frameon=False)
    fig.suptitle('FS coefficient perturbation: per-tone RAPSD\n'
                 '256 x 256; black/white palette; 8bit; seed 0; '
                 'no window or variance normalization', fontsize=13)
    fig.savefig(OUT / 'rapsd.png', dpi=160)
    plt.close(fig)
    for filename, values in [('measurements.csv', rows), ('rapsd-curves.csv', curves)]:
        with (OUT / filename).open('w', newline='') as handle:
            writer = csv.DictWriter(handle, fieldnames=list(values[0]),
                                    lineterminator='\n')
            writer.writeheader()
            writer.writerows(values)
    MANIFEST['spectrum'] = {
        'size': size, 'window': 'none', 'mean_subtracted': True,
        'normalization': 'none', 'dc_power': 0,
        'radial_bin': 'floor(hypot(fftfreq(y), fftfreq(x)) * 256 + 0.5)',
        'peak_ratio': 'max(non-DC radial means) / mean(non-DC radial means)',
        'low_percent': '100 * sum(power[radius < sqrt(g/255)]) / sum(power)',
        'half_tone_limit': 'g=128 cutoff is outside the grid; share saturates',
        'summary_rows': len(rows), 'curve_rows': len(curves),
    }


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, label):
    result = subprocess.run(command, env=ENV, capture_output=True, check=True)
    (OUT / f'{label}.log').write_bytes(result.stderr)
    MANIFEST['commands'].append([portable(arg) for arg in command])
    return result


def source(name, pixels, origin):
    path = OUT / f'{name}-source.png'
    pixels.convert('RGB').save(path)
    MANIFEST['sources'][name] = dict(path=path.name, origin=origin,
                                    sha256=sha(path), size=pixels.size)
    return path


def palette(name, inp, count):
    path = OUT / f'{name}-{count}.pal'
    command = [ENCODER, *COMMON, '--quality=full',
               '--sampling-policy=full-frame', '-Qheckbert',
               '--cover-policy=off', '-p', str(count), '-dnone',
               '-M', str(path), '-o', os.devnull, str(inp)]
    result = run(command, f'{name}-{count}-palette')
    assert b'work=rgb888' in result.stderr
    entries = int(path.read_text().splitlines()[2])
    assert entries == count, (name, entries, count)
    return path


def encode(name, inp, pal, ident, scan, amount, seed=0):
    stem = f'{name}-{ident}'
    six = OUT / f'{stem}.six'
    png = OUT / f'{stem}.png'
    exported = OUT / f'{stem}.pal'
    diffusion = (f'fs:scan={scan}:perturb={amount}:perturb_seed={seed}'
                 ':threads_max=1')
    command = [ENCODER, *COMMON, '-m', str(pal), '-M', str(exported),
               '-d', diffusion, str(inp)]
    result = run(command, stem)
    assert b'work=rgb888' in result.stderr
    six.write_bytes(result.stdout)
    run([DECODER, '-i', str(six), '-o', str(png)], f'{stem}-decode')
    pixels = Image.open(png).convert('RGB')
    assert pixels.size == Image.open(inp).size
    # Exported palette identity is checked across methods by the caller.
    MANIFEST['outputs'].append(dict(
        name=stem, file=png.name, sha256=sha(png), sixel_sha256=sha(six),
        palette_sha256=sha(exported), scan=scan, amount=amount, seed=seed,
        size=pixels.size, pixel_sha256=hashlib.sha256(pixels.tobytes()).hexdigest(),
        bytes=len(result.stdout),
        mean_rgb=np.asarray(pixels).mean((0, 1)).tolist()))
    print(f'encoded {stem}', flush=True)
    return pixels, exported.read_bytes()


def sheet(filename, title, subtitle, headers, rows, zoom=1):
    """Place pixels on integer coordinates; verify every displayed sample."""
    left, top, gap, rowlabel, footer = 20, 124, 16, 38, 38
    cellw = max(im.width for _, cells in rows for im in cells) * zoom
    cellh = max(im.height for _, cells in rows for im in cells) * zoom
    width = left * 2 + len(headers) * cellw + (len(headers)-1) * gap
    height = top + len(rows) * (rowlabel + cellh + gap) + footer
    fig = plt.figure(figsize=(width / 100, height / 100), dpi=100,
                     facecolor='#f3f3f3')
    fig.text(left / width, 1 - 28 / height, title, fontsize=19,
             weight='bold', va='top', color='#181818')
    fig.text(left / width, 1 - 61 / height, subtitle, fontsize=10,
             va='top', color='#444444')
    for col, head in enumerate(headers):
        fig.text((left + col*(cellw+gap)) / width, 1 - 96 / height,
                 head, fontsize=12, weight='bold', va='top')
    checks = []
    for row, (label, cells) in enumerate(rows):
        y = top + row * (rowlabel + cellh + gap)
        fig.text(left / width, 1 - y / height, label, fontsize=11,
                 va='top', color='#333333')
        for col, im in enumerate(cells):
            displayed = im.resize((im.width*zoom, im.height*zoom),
                                  Image.Resampling.NEAREST)
            x = left + col*(cellw+gap)
            yp = y + rowlabel
            fig.figimage(np.asarray(displayed), xo=x,
                         yo=height-yp-displayed.height, origin='upper')
            checks.append((x, yp, displayed))
    fig.text(left / width, 18 / height,
             'Decoded SIXEL pixels | 8-bit gamma RGB | fixed palette | '
             'one worker | seed 0 unless labelled | view PNG at 100%',
             fontsize=9, color='#444444')
    path = OUT / filename
    fig.savefig(path, dpi=100)
    plt.close(fig)
    rendered = Image.open(path).convert('RGB')
    assert rendered.size == (width, height)
    for x, y, expected in checks:
        actual = rendered.crop((x, y, x+expected.width, y+expected.height))
        assert actual.tobytes() == expected.tobytes(), (filename, x, y)
    MANIFEST['sheets'].append(dict(file=filename, size=[width, height],
                                   zoom=zoom, exact_panels=len(checks),
                                   sha256=sha(path)))
    print(f'verified {filename}: {len(checks)} pixel-exact panels', flush=True)


def roi_guide(inp, boxes, filename):
    im = Image.open(inp).convert('RGB')
    fig, ax = plt.subplots(figsize=(9, 6.8), layout='constrained')
    ax.imshow(im)
    for name, (x, y, right, bottom) in boxes:
        ax.add_patch(Rectangle((x, y), right-x, bottom-y, fill=False,
                               edgecolor='#ff00d4', linewidth=2))
        ax.text(x+3, y+4, name, va='top', fontsize=10, color='white',
                bbox=dict(facecolor='#3c223b', alpha=.9, pad=3))
    ax.set_title('Photo source and fixed crop locations (before encoding)')
    ax.axis('off')
    fig.savefig(OUT / filename, dpi=130)
    plt.close(fig)


def main():
    (OUT / 'bw.pal').write_text('JASC-PAL\n0100\n2\n0 0 0\n255 255 255\n')
    bw = OUT / 'bw.pal'
    data = {}
    inputs = {}
    for g in (32, 64, 85, 128):
        name = f'g{g}'
        inputs[name] = source(name, Image.new('RGB', (256, 256), (g,)*3),
                              f'constant gamma RGB = ({g}, {g}, {g})')
    # A shallow monotonic ramp includes long near-constant tone neighborhoods.
    ramp = np.rint(np.linspace(16, 144, 256)).astype(np.uint8)
    inputs['ramp'] = source('ramp', Image.fromarray(np.tile(ramp, (256, 1))),
                           'round(linspace(16,144,256)); repeated for 256 rows')
    photo_path = ROOT / 'images/egret.jpg'
    gradient_path = ROOT / 'images/measurements/palette-pipeline/smooth-gradient-600x450.png'
    inputs['photo'] = source('photo', Image.open(photo_path),
                            dict(path=photo_path.relative_to(ROOT).as_posix(), sha256=sha(photo_path)))
    inputs['color'] = source('color', Image.open(gradient_path),
                            dict(path=gradient_path.relative_to(ROOT).as_posix(), sha256=sha(gradient_path)))
    cases = [(key, path, bw) for key, path in inputs.items()
             if key not in ('photo', 'color')]
    for name in ('photo', 'color'):
        for count in (16, 256):
            pal = palette(name, inputs[name], count)
            cases.append((f'{name}-{count}', inputs[name], pal))
    for name, inp, pal in cases:
        data[name] = {}
        palettes = []
        for ident, label, scan, amount in CONDITIONS:
            pixels, palbytes = encode(name, inp, pal, ident, scan, amount)
            data[name][ident] = pixels
            palettes.append(palbytes)
            if pal == bw:
                assert set(np.unique(np.asarray(pixels))).issubset({0, 255})
        assert len(set(palettes)) == 1, f'palette drift for {name}'
    heads = [label for _, label, _, _ in CONDITIONS]
    keys = [ident for ident, _, _, _ in CONDITIONS]
    sheet('01-flat-native.png', 'Uniform tones: texture at native resolution',
          'Each panel is the full 256 x 256 output, copied 1:1 into this PNG.',
          heads, [(f'g = {g} / 255', [data[f'g{g}'][k] for k in keys])
                  for g in (32, 64, 85, 128)])
    sheet('02-flat-4x.png', 'Uniform tones: inspect periodic patterns and worms',
          'Same central crop (x=96, y=96, 64 x 64); nearest-neighbor 4x. '
          'Judge overall grain in the native sheet too.', heads,
          [(f'g = {g} / 255', [data[f'g{g}'][k].crop((96,96,160,160))
                                for k in keys]) for g in (32,64,85,128)], 4)
    sheet('03-gray-ramp.png', 'A shallow gray ramp: bands, worms, and grain',
          'Input runs from g=16 to 144 across 256 columns; all outputs use '
          'the same black/white palette.', ['Source', *heads],
          [('Full image / native pixels', [Image.open(inputs['ramp']).convert('RGB'),
                                           *[data['ramp'][k] for k in keys]])])
    boxes = [('Background', (48,24,176,152)),
             ('Feather shading', (432,288,560,416)),
             ('Eye and contour', (320,144,448,272))]
    MANIFEST['photo_crops'] = boxes
    roi_guide(inputs['photo'], boxes, '04-photo-crop-locations.png')
    for count in (16,256):
        ref = Image.open(inputs['photo']).convert('RGB')
        sheet(f'05-photo-{count}-2x.png',
              f'Photo: {count} fixed colors / three texture-sensitive regions',
              '128 x 128 crops taken AFTER full-frame encoding; '
              'nearest-neighbor 2x. Source has full RGB color.', ['Source', *heads],
              [(label, [im.crop(box) for im in [ref, *[
                  data[f'photo-{count}'][k] for k in keys]]])
                  for label, box in boxes], 2)
        color = Image.open(inputs['color']).convert('RGB')
        sheet(f'06-color-{count}-2x.png', f'Color gradient: {count} fixed colors',
              '128 x 128 crop at (224,160), taken after full-frame encoding; '
              'nearest-neighbor 2x.', ['Source', *heads],
              [('Smooth color transitions', [im.crop((224,160,352,288))
                  for im in [color, *[data[f'color-{count}'][k] for k in keys]]])], 2)
    sweep_rows = []
    seed_rows = []
    for g in (64,128):
        name = f'g{g}'
        p25, _ = encode(name, inputs[name], bw, 'fs-025', 'raster', .25)
        p1, _ = encode(name, inputs[name], bw, 'fs-1', 'raster', 1)
        seeds = [data[name]['fs-05']]
        for seed in (1,2):
            im, _ = encode(name, inputs[name], bw, f'fs-05-seed{seed}',
                           'raster', .5, seed)
            seeds.append(im)
        sweep_rows.append((f'g = {g} / 255', [im.crop((96,96,160,160))
            for im in [data[name]['fs-0'], p25, data[name]['fs-05'], p1]]))
        seed_rows.append((f'g = {g} / 255', [im.crop((96,96,160,160))
                                            for im in seeds]))
    sheet('07-strength-4x.png', 'Perturbation strength: the texture tradeoff',
          'Raster FS, fixed seed 0; same central 64 x 64 crop, nearest-neighbor 4x.',
          ['P=0', 'P=0.25', 'P=0.5', 'P=1.0'], sweep_rows, 4)
    sheet('08-seeds-4x.png', 'Seed controls: compare three patterns',
          'Raster FS, P=0.5; same central 64 x 64 crop, nearest-neighbor 4x.',
          ['Seed 0', 'Seed 1', 'Seed 2'], seed_rows, 4)
    spectra(data)
    (OUT / 'manifest.json').write_text(json.dumps(MANIFEST, indent=2)+'\n')
    print(f"Done: {len(MANIFEST['outputs'])} decoded outputs, "
          f"{len(MANIFEST['sheets'])} verified comparison sheets.", flush=True)


if __name__ == '__main__':
    configure()
    main()
