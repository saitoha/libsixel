#!/usr/bin/env python3
"""Time Pillow GIF and PNG encoding stage by stage for stages.py.

Run with a Python whose Pillow was built with pillow_stage_timers.py. Reads
raw RGB crops named NAME@WxH.rgb from argv[1], times argv[2] samples after
two warm-ups, and prints JSON. Stage timers accumulate inside Pillow's C
code; the remainder of each call (pixel copies, container writing and the
Python wrapper) is reported as 'other'.
"""
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import time

from PIL import Image, _imaging


def gif(image):
    _imaging._stage_times()
    output = io.BytesIO()
    start = time.perf_counter_ns()
    image.quantize(colors=256, method=Image.Quantize.MEDIANCUT).save(output, 'GIF', optimize=False)
    total = (time.perf_counter_ns() - start) / 1e6
    t = _imaging._stage_times()
    stages = {'sampling': t[1], 'palette search': t[2], 'lookup': t[3], 'LZW encode': t[4]}
    stages['other'] = total - sum(stages.values())
    return stages, total, output.getvalue()


def png(image):
    _imaging._stage_times()
    output = io.BytesIO()
    start = time.perf_counter_ns()
    image.save(output, 'PNG', compress_level=6)
    total = (time.perf_counter_ns() - start) / 1e6
    t = _imaging._stage_times()
    stages = {'filter': t[5] - t[6], 'DEFLATE': t[6]}
    stages['other'] = total - sum(stages.values())
    return stages, total, output.getvalue()


def main():
    if not hasattr(_imaging, '_stage_times'):
        raise SystemExit('this Pillow lacks pillow_stage_timers.py instrumentation')
    runs = int(sys.argv[2])
    rows = []
    for path in sorted(Path(sys.argv[1]).glob('*.rgb')):
        name, w, h = re.fullmatch(r'(.+)@(\d+)x(\d+)\.rgb', path.name).groups()
        image = Image.frombytes('RGB', (int(w), int(h)), path.read_bytes())
        for codec, fn in [('GIF', gif), ('PNG', png)]:
            samples = []
            for n in range(runs + 2):
                stages, total, data = fn(image)
                if n >= 2:
                    samples.append({'stages': stages, 'total_ms': total, 'call_ms': total})
            rows.append({'image': name, 'size': [int(w), int(h)], 'codec': codec, 'samples': samples,
                         'stream_sha256': hashlib.sha256(data).hexdigest()})
    json.dump({'pillow': Image.__version__, 'rows': rows}, sys.stdout)


if __name__ == '__main__':
    main()
