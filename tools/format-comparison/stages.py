#!/usr/bin/env python3
"""Measure where one-thread encoding time goes, stage by stage.

SIXEL stages come from libsixel's own timeline log (SIXEL_LOG_PATH) of the
same in-memory encoder call that measure.py times. GIF and PNG stages come
from a Pillow of the same version built with pillow_stage_timers.py, run by
stages_pillow.py under --pillow-python; its GIF and PNG bytes are checked
against this process's Pillow, so the breakdown describes the measured
implementation.

Each photograph is cropped at 1:1 to every requested size from the
uncropped originals, as in measure.py's size sweep. Requires the same
dependencies and isolated build as measure.py.
"""
import argparse
import datetime
import io
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import tempfile
import time

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import measure  # noqa: E402

CODECS = ['SIXEL default', 'SIXEL quality', 'GIF', 'PNG']
# Stage names in display order. A codec reports only the stages it has.
STAGES = {
    'SIXEL': ['color conversion', 'sampling', 'palette search',
              'lookup setup', 'lookup + dither', 'SIXEL encode', 'other'],
    'GIF': ['sampling', 'palette search', 'lookup', 'LZW encode', 'other'],
    'PNG': ['filter', 'DEFLATE', 'other'],
}


def read_events(path, offset):
    """Return the JSONL records appended to `path` after byte `offset`."""
    with open(path, 'rb') as stream:
        stream.seek(offset)
        return [json.loads(line) for line in stream.read().splitlines() if line.strip()]


def first(events, worker, role, event):
    for e in events:
        if e['worker'] == worker and e['role'] == role and e['event'] == event:
            return e['ts']
    return None


def sixel_marks(events):
    """Collect the milestones of one encode from its timeline.

    The encoding thread is the one that logs encode_begin. Working-space
    conversion (-W) is the colorspace-start/finish envelope on that thread;
    the clustering conversion (-X) of an asynchronous palette job runs on
    the job's thread and belongs to sampling, as do sample extraction and
    palette initialization up to the first refinement step.
    """
    begin = next(e for e in events if e['worker'] == 'main' and e['event'] == 'encode_begin')
    main = begin['thread']
    marks = {'begin': begin['ts'],
             'done': first(events, 'main', 'encoder', 'encode_done'),
             'sample': (first(events, 'worker', 'filter', 'sample-start'),
                        first(events, 'worker', 'filter', 'sample-finish')),
             'pal_start': first(events, 'palette/build', 'palette', 'start'),
             'pal_finish': first(events, 'palette/build', 'palette', 'finish'),
             'dither': first(events, 'worker', 'filter', 'dither-prepare'),
             'bands': first(events, 'pipeline', 'controller', 'configure'),
             'end': max(e['ts'] for e in events if e['worker'] == 'encode'
                        and e['role'] == 'worker' and e['event'] == 'finish')}
    marks['search'] = first(events, 'palette/build', 'palette/iterate', 'start')
    if marks['search'] is None:
        marks['search'] = first(events, 'palette/build', 'palette/init', 'finish')
    working, clustering = [], []
    opened = {}
    for e in events:
        if e['worker'] != 'worker' or e['role'] != 'filter':
            continue
        if e['event'] == 'colorspace-start':
            opened[e['thread']] = e['ts']
        elif e['event'] == 'colorspace-finish' and e['thread'] in opened:
            pair = (opened.pop(e['thread']), e['ts'])
            (working if e['thread'] == main else clustering).append(pair)
    marks['working'] = working
    marks['clustering'] = clustering
    order = [marks['begin'], marks['sample'][0], marks['sample'][1], marks['pal_start'],
             marks['search'], marks['pal_finish'], marks['dither'], marks['end'], marks['done']]
    assert None not in order and order == sorted(order), order
    return marks


def sixel_stages(events):
    """Split one single-worker encode into stages from its timeline."""
    m = sixel_marks(events)
    assert m['bands'] is not None and m['dither'] <= m['bands'] <= m['end']
    conversion_end = max([m['pal_finish']] + [hi for _, hi in m['working']])
    stages = {
        'color conversion': sum(hi - lo for lo, hi in m['working']),
        'sampling': (m['sample'][1] - m['sample'][0])
                    + sum(hi - lo for lo, hi in m['clustering'])
                    + (m['search'] - m['pal_start']),
        'palette search': m['pal_finish'] - m['search'],
        'lookup setup': m['dither'] - conversion_end,
        'lookup + dither': m['bands'] - m['dither'],
        'SIXEL encode': m['end'] - m['bands'],
    }
    stages['other'] = (m['done'] - m['begin']) - sum(stages.values())
    assert stages['lookup setup'] >= 0 and stages['other'] >= -1e-9, stages
    return {k: v * 1e3 for k, v in stages.items()}, (m['done'] - m['begin']) * 1e3


# Wall-clock segments of a SIXEL encode at any worker budget, in stacking
# order. With several workers the color conversion can run beside palette
# construction, and lookup, diffusion and band encoding form one pipeline.
SEGMENTS = ['color conversion', 'sampling', 'sampling | conversion',
            'palette search', 'palette search | conversion', 'lookup setup',
            'lookup + dither', 'SIXEL encode', 'lookup + dither + encode', 'other']


def sixel_segments(events, threads):
    """Partition one encode's wall clock by the activities running in it.

    Every interval between consecutive milestones is labeled with the active
    stage; palette intervals that overlap the working-space conversion carry
    a '| conversion' label so the stacked bar still sums to the elapsed time.
    With one worker the lookup and band phases are separated at the pipeline
    controller; with more workers they overlap and are reported together.
    """
    m = sixel_marks(events)
    conversion_end = max([m['pal_finish']] + [hi for _, hi in m['working']])
    activities = [('sampling', m['sample'][0], m['sample'][1])]
    activities += [('sampling', lo, hi) for lo, hi in m['clustering']]
    activities += [('sampling', m['pal_start'], m['search']),
                   ('palette search', m['search'], m['pal_finish']),
                   ('lookup setup', conversion_end, m['dither'])]
    if threads == 1:
        assert m['dither'] <= m['bands'] <= m['end']
        activities += [('lookup + dither', m['dither'], m['bands']),
                       ('SIXEL encode', m['bands'], m['end'])]
    else:
        activities.append(('lookup + dither + encode', m['dither'], m['end']))
    points = {m['begin'], m['done']}
    for _, lo, hi in activities:
        points |= {lo, hi}
    for lo, hi in m['working']:
        points |= {lo, hi}
    marks = sorted(points)
    segments = dict.fromkeys(SEGMENTS, 0.0)
    for a, b in zip(marks, marks[1:]):
        middle = (a + b) / 2
        converting = any(lo <= middle < hi for lo, hi in m['working'])
        label = next((name for name, lo, hi in activities if lo <= middle < hi), None)
        if label in ('sampling', 'palette search') and converting:
            label += ' | conversion'
        elif label is None:
            label = 'color conversion' if converting else 'other'
        segments[label] += (b - a) * 1e3
    return segments, (m['done'] - m['begin']) * 1e3


def pillow_reference(image, codec):
    output = io.BytesIO()
    if codec == 'PNG':
        image.save(output, 'PNG', compress_level=6)
    else:
        image.quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(output, 'GIF', optimize=False)
    return measure.sha(output.getvalue())


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--adapter', type=Path, required=True)
    p.add_argument('--library', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--revision', required=True)
    p.add_argument('--photos', type=Path, required=True,
                   help='directory of uncropped originals (*.jpg)')
    p.add_argument('--images', default='photo-001,photo-011,photo-021,photo-041,photo-051,photo-061,photo-071,photo-081,photo-091')
    p.add_argument('--crops', default='640x480,1920x1080')
    p.add_argument('--runs', type=int, default=7)
    p.add_argument('--thread-crop', default='1920x1080',
                   help='crop size of the worker-budget comparison')
    p.add_argument('--thread-budgets', default='1,8')
    p.add_argument('--pillow-python', required=True,
                   help='Python whose Pillow carries pillow_stage_timers.py')
    args = p.parse_args()
    for key in list(os.environ):
        if key.startswith(('SIXEL_', 'LSQA_')):
            del os.environ[key]
    work = Path(tempfile.mkdtemp(prefix='format-stages-'))
    log = work / 'timeline.jsonl'
    # The library probes the log path once, so set it before loading.
    os.environ.update(SIXEL_GPU_POLICY='off', SIXEL_PALETTE_KMEANS_SEED='1', SIXEL_LOG_PATH=str(log))
    names = args.images.split(',')
    crops = [tuple(int(v) for v in c.lower().split('x')) for c in args.crops.split(',')]
    images, sources = measure.crop_fixtures(args.photos, names, crops)
    codecs = measure.Codecs(args.adapter.resolve(), args.library.resolve())
    rows = []
    for (name, crop), image in images.items():
        for codec in CODECS[:2]:
            samples = []
            for n in range(args.runs + 2):
                offset = log.stat().st_size if log.exists() else 0
                start = time.perf_counter_ns()
                codecs.encode(image, codec, 1)
                wall = (time.perf_counter_ns() - start) / 1e6
                stages, total = sixel_stages(read_events(log, offset))
                if n >= 2:
                    samples.append({'stages': stages, 'total_ms': total, 'call_ms': wall})
            rows.append({'image': name, 'size': list(crop), 'codec': codec, 'samples': samples})
            print(name, crop, codec, round(statistics.median(s['total_ms'] for s in samples), 2), flush=True)
    thread_crop = tuple(int(v) for v in args.thread_crop.lower().split('x'))
    budgets = [int(v) for v in args.thread_budgets.split(',')]
    thread_rows = []
    for (name, crop), image in images.items():
        if crop != thread_crop:
            continue
        for codec in CODECS[:2]:
            for threads in budgets:
                samples = []
                for n in range(args.runs + 2):
                    offset = log.stat().st_size if log.exists() else 0
                    start = time.perf_counter_ns()
                    codecs.encode(image, codec, threads)
                    wall = (time.perf_counter_ns() - start) / 1e6
                    segments, total = sixel_segments(read_events(log, offset), threads)
                    if n >= 2:
                        samples.append({'segments': segments, 'total_ms': total, 'call_ms': wall})
                thread_rows.append({'image': name, 'size': list(crop), 'codec': codec,
                                    'threads': threads, 'samples': samples})
                print(name, crop, codec, threads, round(statistics.median(s['total_ms'] for s in samples), 2), flush=True)
    raw = work / 'raw'
    raw.mkdir()
    for (name, (w, h)), image in images.items():
        (raw / f'{name}@{w}x{h}.rgb').write_bytes(image.tobytes())
    helper = Path(__file__).with_name('stages_pillow.py')
    pillow = json.loads(subprocess.run([args.pillow_python, str(helper), str(raw), str(args.runs)],
                                       check=True, capture_output=True, text=True).stdout)
    for row in pillow['rows']:
        image = images[row['image'], tuple(row['size'])]
        # The instrumented build must encode exactly what this Pillow does.
        assert row.pop('stream_sha256') == pillow_reference(image, row['codec']), row['image']
        rows.append(row)
    payload = {'metadata': {
        'revision': args.revision, 'platform': platform.platform(),
        'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'python': platform.python_version(), 'pillow': Image.__version__,
        'instrumented_pillow': pillow['pillow'],
        'pillow_patch_sha256': measure.sha(Path(__file__).with_name('pillow_stage_timers.py').read_bytes()),
        'runs': args.runs, 'warmups': 2, 'threads': 1, 'codecs': CODECS, 'stages': STAGES,
        'images': names, 'sizes': [list(c) for c in crops],
        'thread_size': list(thread_crop), 'thread_budgets': budgets, 'segments': SEGMENTS,
        'photo_directory': str(args.photos), 'photo_sources': sources,
        'inputs': {f'{k[0]}@{k[1][0]}x{k[1][1]}': measure.sha(v.tobytes()) for k, v in images.items()},
        'script_sha256': measure.sha(Path(__file__).read_bytes()),
        'library_sha256': measure.sha(args.library.read_bytes())},
        'rows': rows, 'thread_rows': thread_rows}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'stages.json').write_text(json.dumps(payload, indent=2) + '\n')


if __name__ == '__main__':
    main()
