#!/usr/bin/env python3
"""Measure the two writer branches in isolation and regenerate PNG charts.

Requires a configured, built shared libsixel, a C compiler, pkg-config libpng,
Pillow and matplotlib. --measure compiles only writer.c twice, keeping the
support library and config identical. Normal invocation reuses recorded data.
"""
import argparse
import ctypes
import hashlib
import io
import json
import os
import platform
import shlex
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/writers/png-figures'


def command(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()


def measure():
    """Exclude fixture preparation, compilation and decoding from timings."""
    os.environ.pop('SIXEL_TIMELINE', None)
    if any('TIMELINE' in key and key.startswith('SIXEL') for key in os.environ):
        raise RuntimeError('Unset SIXEL timeline environment variables first')
    library = ROOT / 'src/.libs' / (
        'libsixel.dylib' if platform.system() == 'Darwin' else 'libsixel.so')
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    flags = shlex.split(command('pkg-config', '--cflags', '--libs', 'libpng'))
    photo = Image.open(ROOT / 'images/snake.png').convert('RGB')
    photo = photo.resize((512, 512), Image.Resampling.LANCZOS)
    blocks = Image.new('RGB', (512, 512), '#ffffff')
    draw = ImageDraw.Draw(blocks)
    for i, color in enumerate(['#164e63', '#0284c7', '#eab308', '#be185d']):
        draw.rectangle((32, 32 + i * 112, 480 - i * 64, 112 + i * 112), fill=color)
    result = {'metadata': {
        'revision': command('git', 'rev-parse', 'HEAD'),
        'writer_sha256': hashlib.sha256((ROOT / 'src/writer.c').read_bytes()).hexdigest(),
        'config_sha256': hashlib.sha256((ROOT / 'config.h').read_bytes()).hexdigest(),
        'library_sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
        'fixture_sha256': hashlib.sha256((ROOT / 'images/snake.png').read_bytes()).hexdigest(),
        'platform': platform.platform(),
        'cpu': command('sysctl', '-n', 'machdep.cpu.brand_string') if platform.system() == 'Darwin' else platform.processor(),
        'compiler': command(*compiler, '--version').splitlines()[0],
        'libpng': command('pkg-config', '--modversion', 'libpng'),
        'pillow': Image.__version__, 'compile_flags': '-O2 -fPIC -shared',
        'dimensions': [512, 512], 'warmups': 3, 'samples': 21,
        'timing': 'perf_counter_ns around one public writer call to /dev/null; alternating backend order',
        'size': 'complete PNG file; decoded pixels checked against input',
    }, 'rows': []}
    with tempfile.TemporaryDirectory(prefix='libsixel-png-') as temp:
        temp = Path(temp)
        writers = {}
        for backend, enabled in [('builtin', 0), ('libpng', 1)]:
            wrapper = temp / (backend + '.c')
            # Override only the writer backend after loading the real config.
            wrapper.write_text('#include "config.h"\n#undef HAVE_CONFIG_H\n'
                               '#undef HAVE_LIBPNG\n#define HAVE_LIBPNG %d\n'
                               '#define sixel_helper_write_image_file benchmark_write\n'
                               '#include "src/writer.c"\n' % enabled)
            target = temp / (backend + '.so')
            subprocess.run(compiler + ['-O2', '-fPIC', '-shared', '-I.', '-Iinclude',
                           '-Isrc', str(wrapper), str(library), '-o', str(target)]
                           + flags, cwd=ROOT, check=True)
            function = ctypes.CDLL(str(target)).benchmark_write
            function.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                 ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p,
                                 ctypes.c_int, ctypes.c_void_p]
            function.restype = ctypes.c_int
            writers[backend] = function
        for name, source in [('Photo', photo), ('Flat shapes', blocks)]:
            for mode, code in [('PAL8', 131), ('RGB', 3), ('RGBA', 17)]:
                frame = source.quantize(colors=256) if mode == 'PAL8' else source.convert(mode)
                if mode == 'RGBA':
                    # Exercise nonconstant alpha as well as color samples.
                    frame.putalpha(Image.frombytes('L', (512, 512), bytes(range(256)) * 1024))
                pixels = ctypes.create_string_buffer(frame.tobytes())
                palette = ctypes.create_string_buffer(bytes(frame.getpalette()).ljust(768, b'\0')) if mode == 'PAL8' else None
                timings = {key: [] for key in writers}
                for iteration in range(24):
                    order = list(writers) if iteration % 2 == 0 else list(reversed(writers))
                    for backend in order:
                        start = time.perf_counter_ns()
                        status = writers[backend](pixels, 512, 512, palette, code, b'/dev/null', 1, None)
                        elapsed = (time.perf_counter_ns() - start) / 1e6
                        if status != 0:
                            raise RuntimeError((backend, status))
                        if iteration >= 3:
                            timings[backend].append(elapsed)
                for backend, writer in writers.items():
                    output = temp / 'output.png'
                    assert writer(pixels, 512, 512, palette, code, os.fsencode(output), 1, None) == 0
                    decoded = Image.open(output)
                    assert decoded.convert('RGBA').tobytes() == frame.convert('RGBA').tobytes()
                    result['rows'].append({'fixture': name, 'format': mode, 'backend': backend,
                                           'milliseconds': timings[backend], 'bytes': output.stat().st_size})
    return result


def render(data, metric):
    """Use separate axes per format; bars always start at zero."""
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 11,
                         'svg.hashsalt': 'libsixel-png-writer'})
    fig, axes = plt.subplots(6, 1, figsize=(8, 13), layout='constrained')
    cases = [(mode, fixture) for mode in ['PAL8', 'RGB', 'RGBA']
             for fixture in ['Photo', 'Flat shapes']]
    for ax, (mode, fixture) in zip(axes, cases):
        for offset, backend, color in [(-0.18, 'builtin', '#176b99'), (0.18, 'libpng', '#b4560b')]:
            rows = [r for r in data['rows'] if r['format'] == mode and r['fixture'] == fixture and r['backend'] == backend]
            values = [statistics.median(r['milliseconds']) if metric == 'time' else r['bytes'] / 1024 for r in rows]
            positions = [i + offset for i in range(len(rows))]
            ax.barh(positions, values, height=.31, label=backend, color=color)
            if metric == 'time':
                errors = [[v - min(r['milliseconds']) for v, r in zip(values, rows)],
                          [max(r['milliseconds']) - v for v, r in zip(values, rows)]]
                ax.errorbar(values, positions, xerr=errors, fmt='none', ecolor='#222222', capsize=3)
            for y, value, row in zip(positions, values, rows):
                label_x = max(row['milliseconds']) if metric == 'time' else value
                ax.annotate(f'{value:.2f}', (label_x, y), xytext=(8, 0), textcoords='offset points', va='center')
        ax.set_yticks([])
        ax.invert_yaxis()
        ax.set_title(f'{mode} · {fixture}', loc='left', fontweight='bold')
        ax.set_xlabel('Writer time (ms) — lower is faster' if metric == 'time' else 'PNG file size (KiB) — lower is smaller')
        ax.set_xlim(0, ax.get_xlim()[1] * 1.24)
        ax.spines[['top', 'right']].set_visible(False)
        ax.set_axisbelow(True)
        ax.grid(axis='x', color='#dddddd', linewidth=.6)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='outside upper right', ncol=2, frameon=False)
    fig.suptitle('PNG writer: ' + ('speed' if metric == 'time' else 'output size'), x=.02, ha='left', fontweight='bold')
    fig.supxlabel('512 × 512 pixels · ' + ('21 writes; median and observed min–max' if metric == 'time' else 'Complete PNG files; identical decoded pixels'), fontsize=10)
    assets = {}
    for extension in ['svg', 'png']:
        buffer = io.BytesIO()
        fig.savefig(buffer, format=extension, dpi=150, metadata={'Date': None} if extension == 'svg' else {})
        payload = buffer.getvalue()
        if extension == 'svg':
            payload = ('\n'.join(line.rstrip() for line in payload.decode().splitlines()) + '\n').encode()
        assets[f'writer-{metric}.{extension}'] = payload
    plt.close(fig)
    return assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--measure', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.measure and args.check:
        parser.error('--measure and --check are mutually exclusive')
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / 'measurements.json'
    if args.measure:
        path.write_text(json.dumps(measure(), indent=2) + '\n')
    data = json.loads(path.read_text())
    for metric in ['time', 'size']:
        for name, payload in render(data, metric).items():
            destination = OUT / name
            if args.check:
                if destination.read_bytes() != payload:
                    raise RuntimeError('Stale chart: ' + name)
            else:
                destination.write_bytes(payload)


if __name__ == '__main__':
    main()
