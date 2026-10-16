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


def sixel_stages(events):
    """Split one single-worker encode into stages from its timeline.

    With one worker the encoder runs serially: optional working-space color
    conversion, palette construction (its initialization or histogram until
    the first refinement step, then the search itself), lookup preparation
    up to the dither-prepare mark, palette application with error diffusion,
    and finally the six-row band jobs announced by the pipeline controller.
    The order is asserted so that a changed event layout fails loudly.
    """
    begin = first(events, 'main', 'encoder', 'encode_begin')
    done = first(events, 'main', 'encoder', 'encode_done')
    cs_start = first(events, 'worker', 'filter', 'colorspace-start')
    cs_finish = first(events, 'worker', 'filter', 'colorspace-finish')
    pal_start = first(events, 'palette/build', 'palette', 'start')
    pal_finish = first(events, 'palette/build', 'palette', 'finish')
    search = first(events, 'palette/build', 'palette/iterate', 'start')
    if search is None:
        search = first(events, 'palette/build', 'palette/init', 'finish')
    dither = first(events, 'worker', 'filter', 'dither-prepare')
    bands = first(events, 'pipeline', 'controller', 'configure')
    end = max(e['ts'] for e in events if e['worker'] == 'encode' and e['role'] == 'worker'
              and e['event'] == 'finish')
    marks = [begin, pal_start, search, pal_finish, dither, bands, end, done]
    assert all(m is not None for m in marks) and marks == sorted(marks), marks
    if cs_start is not None:
        assert begin <= cs_start <= cs_finish <= pal_start, (cs_start, cs_finish, pal_start)
    stages = {
        'color conversion': (cs_finish - cs_start) if cs_start is not None else 0.0,
        'sampling': search - pal_start,
        'palette search': pal_finish - search,
        'lookup setup': dither - pal_finish,
        'lookup + dither': bands - dither,
        'SIXEL encode': end - bands,
    }
    stages['other'] = (done - begin) - sum(stages.values())
    return {k: v * 1e3 for k, v in stages.items()}, (done - begin) * 1e3


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
        'photo_directory': str(args.photos), 'photo_sources': sources,
        'inputs': {f'{k[0]}@{k[1][0]}x{k[1][1]}': measure.sha(v.tobytes()) for k, v in images.items()},
        'script_sha256': measure.sha(Path(__file__).read_bytes()),
        'library_sha256': measure.sha(args.library.read_bytes())},
        'rows': rows}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'stages.json').write_text(json.dumps(payload, indent=2) + '\n')


if __name__ == '__main__':
    main()
