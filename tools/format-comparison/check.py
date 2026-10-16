#!/usr/bin/env python3
"""Validate the published matrix, lossless contracts and fixed-stream scaling."""
import hashlib
import json
import math
from pathlib import Path
import sys

root = Path(sys.argv[1])
p = json.loads((root / 'results.json').read_text())
meta = p['metadata']
rows = p['rows']
n = meta['generations']
settings = meta['settings']
generation_codecs = ['SIXEL default', 'GIF', 'PNG', 'QOI', 'JPEG 80', 'WebP 80', 'WebP lossless', 'AVIF 80', 'HEIC 80']
images = set(meta['inputs'])
workloads = [set(meta['photo_sources']), set(meta.get('chart_sources') or {})]
assert images == workloads[0] | workloads[1] and not workloads[0] & workloads[1]
generation_images = set(meta['generation_images'])
assert generation_images <= images
assert all(generation_images & w for w in workloads if w)
sweep = meta['size_sweep']
sweep_sizes = [tuple(c) for c in sweep['sizes']]
assert set(sweep['images']) <= images
assert len(rows) == (len(images) * len(settings) * 4 + len(generation_images) * len(generation_codecs) * n
                     + len(sweep['images']) * len(sweep_sizes) * len(settings) * len(sweep['threads']))
assert {r['image'] for r in rows if r['experiment'] == 'generation'} == generation_images
keys = set()
for row in rows:
    key = (row['image'], row['experiment'], row['codec'], row['threads'], row.get('mode'), row.get('generation'), tuple(row.get('size', ())))
    assert key not in keys, key
    keys.add(key)
    for field in ['encode_ms', 'decode_ms']:
        assert len(row[field]) == meta['runs']
        assert all(math.isfinite(x) and x > 0 for x in row[field])
    assert row['bytes'] > 0
    assert row['transport_bytes'] == (row['bytes'] if row['codec'].startswith('SIXEL') else 4 * ((row['bytes'] + 2) // 3))
    if row['experiment'] == 'size':
        # The sweep times codecs only; quality is the corpus's job.
        assert tuple(row['size']) in sweep_sizes and 'ms_ssim' not in row, key
        continue
    assert 0 <= row['ms_ssim'] <= 1.000001
    assert math.isfinite(row['delta_e']) and row['delta_e'] >= 0
    if row['codec'] in ['PNG', 'QOI', 'WebP lossless']:
        assert row['exact_input'], key
        assert row['ms_ssim'] == 1, key
    if row['experiment'] == 'generation':
        palette = row['codec'] in ['SIXEL default', 'GIF']
        assert row['mode'] == ('indexed' if palette else 'RGB')
        if palette:
            assert row['exact_generation_one'], key
# Inputs are not stored; they are identified by source and input digests
# and must be read from the dataset directories to be regenerated.
for name in images:
    for codec in settings:
        group = [r for r in rows if r['image'] == name and r['codec'] == codec and r['experiment'] == 'scaling']
        assert sorted(r['threads'] for r in group) == [1, 2, 4, 8], (name, codec)
        assert len({r['decoder_stream_sha256'] for r in group}) == 1, (name, codec)
for name in sweep['images']:
    for size in sweep_sizes:
        for codec in settings:
            group = [r for r in rows if r['experiment'] == 'size' and r['image'] == name
                     and tuple(r['size']) == size and r['codec'] == codec]
            assert sorted(r['threads'] for r in group) == sweep['threads'], (name, size, codec)
            assert len({r['decoder_stream_sha256'] for r in group}) == 1, (name, size, codec)
stages_path = root / 'stages.json'
if stages_path.exists():
    stages = json.loads(stages_path.read_text())
    smeta = stages['metadata']
    assert len(stages['rows']) == len(smeta['images']) * len(smeta['sizes']) * len(smeta['codecs'])
    for row in stages['rows']:
        names = smeta['stages'][row['codec'].split()[0]]
        assert len(row['samples']) == smeta['runs'], row['image']
        for sample in row['samples']:
            assert list(sample['stages']) == names, (row['codec'], list(sample['stages']))
            assert all(math.isfinite(v) and v >= 0 for v in sample['stages'].values()), (row['image'], row['codec'], sample)
            assert math.isclose(sum(sample['stages'].values()), sample['total_ms'], rel_tol=1e-9, abs_tol=1e-9)
print(f'validated {len(rows)} records for {len(images)} images, lossless pixels and retained-index generations')
