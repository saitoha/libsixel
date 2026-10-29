#!/usr/bin/env python3
"""Validate the published matrix, lossless contracts and fixed-stream scaling."""
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
assert settings == ['SIXEL -d none', 'GIF', 'PNG', 'QOI', 'JPEG 80', 'WebP 80',
                    'WebP lossless', 'AVIF 80', 'HEIC 80'], settings
metrics = {'ms_ssim': 'MS-SSIM', 'scielab': 'S-CIELAB',
           'delta_e': 'Δ E00_mean', 'scielab_max': 'S-CIELAB_max'}
assert meta['quality_metrics'] == metrics
assert meta['scielab_ppd_requested'] == 60
assert meta['scielab_ppd'] in (None, 60)
generation_codecs = settings
images = set(meta['inputs'])
workloads = [set(meta['photo_sources']), set(meta.get('chart_sources') or {})]
assert images == workloads[0] | workloads[1] and not workloads[0] & workloads[1]
generation_images = set(meta['generation_images'])
assert generation_images <= images
assert all(generation_images & w for w in workloads if w)
sweep = meta['size_sweep']
sweep_sizes = [tuple(c) for c in sweep['sizes']]
assert set(sweep['images']) <= images
assert sweep['threads'] == [1, 8]
assert len(rows) == (len(images) * len(settings) * 4 + len(generation_images) * len(generation_codecs) * n
                     + len(sweep['images']) * len(sweep_sizes) * len(settings) * len(sweep['threads']))
assert {r['image'] for r in rows if r['experiment'] == 'generation'} == generation_images
keys = set()
common = {'image', 'experiment', 'codec', 'threads', 'encode_ms', 'decode_ms',
          'bytes', 'transport_bytes', 'stream_sha256'}
quality_fields = set(metrics) | {'rgb_sha256', 'exact_input'}
fields = {'scaling': common | quality_fields | {'decoder_stream_sha256'},
          'generation': common | quality_fields | {'mode', 'generation', 'exact_generation_one'},
          'size': common | {'size', 'decoder_stream_sha256'}}
for row in rows:
    key = (row['image'], row['experiment'], row['codec'], row['threads'], row.get('mode'), row.get('generation'), tuple(row.get('size', ())))
    assert row['experiment'] in fields, key
    assert set(row) == fields[row['experiment']], (key, set(row) ^ fields[row['experiment']])
    assert row['codec'] in settings and row['image'] in images, key
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
    for field in ['scielab', 'delta_e', 'scielab_max']:
        assert math.isfinite(row[field]) and row[field] >= 0, (key, field)
    assert row['scielab_max'] >= row['scielab'], key
    if row['codec'] in ['PNG', 'QOI', 'WebP lossless']:
        assert row['exact_input'], key
        assert row['ms_ssim'] == 1, key
    if row['experiment'] == 'generation':
        palette = row['codec'] in ['SIXEL -d none', 'GIF']
        assert row['threads'] == 1 and 1 <= row['generation'] <= n, key
        assert row['mode'] == ('indexed' if palette else 'RGB')
        if palette:
            assert row['exact_generation_one'], key
for name in generation_images:
    for codec in generation_codecs:
        group = [r for r in rows if r['experiment'] == 'generation'
                 and r['image'] == name and r['codec'] == codec]
        assert sorted(r['generation'] for r in group) == list(range(1, n + 1)), (name, codec)
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
    assert smeta['codecs'] == ['SIXEL -d none', 'GIF', 'PNG'], smeta['codecs']
    assert smeta['stages'] == {
        'SIXEL': ['color conversion', 'sampling', 'palette search',
                  'lookup setup', 'lookup', 'SIXEL encode', 'other'],
        'GIF': ['sampling', 'palette search', 'lookup', 'LZW encode', 'other'],
        'PNG': ['filter', 'DEFLATE', 'other']}
    assert smeta['segments'] == ['color conversion', 'sampling', 'sampling | conversion',
                                'palette search', 'palette search | conversion', 'lookup setup',
                                'lookup', 'SIXEL encode', 'lookup + encode', 'other']
    assert smeta['revision'] == meta['revision']
    assert smeta['library_sha256'] == meta['library_sha256']
    assert len(stages['rows']) == len(smeta['images']) * len(smeta['sizes']) * len(smeta['codecs'])
    stage_keys = set()
    for row in stages['rows']:
        assert set(row) == {'image', 'size', 'codec', 'samples'}, row
        key = (row['image'], tuple(row['size']), row['codec'])
        assert key not in stage_keys, key
        stage_keys.add(key)
        names = smeta['stages'][row['codec'].split()[0]]
        assert len(row['samples']) == smeta['runs'], row['image']
        for sample in row['samples']:
            assert set(sample) == {'stages', 'total_ms', 'call_ms'}, key
            assert list(sample['stages']) == names, (row['codec'], list(sample['stages']))
            assert all(math.isfinite(v) and v >= 0 for v in sample['stages'].values()), (row['image'], row['codec'], sample)
            assert math.isclose(sum(sample['stages'].values()), sample['total_ms'], rel_tol=1e-9, abs_tol=1e-9)
            assert math.isfinite(sample['call_ms']) and sample['call_ms'] > 0, key
    assert stage_keys == {(name, tuple(size), codec) for name in smeta['images']
                          for size in smeta['sizes'] for codec in smeta['codecs']}
    thread_keys = set()
    for row in stages['thread_rows']:
        assert set(row) == {'image', 'size', 'codec', 'threads', 'samples'}, row
        key = (row['image'], row['threads'], row['codec'])
        assert key not in thread_keys and row['size'] == smeta['thread_size'], key
        thread_keys.add(key)
        assert len(row['samples']) == smeta['runs'], key
        for sample in row['samples']:
            assert set(sample) == {'segments', 'total_ms', 'call_ms'}, key
            assert list(sample['segments']) == smeta['segments'], key
            assert all(math.isfinite(v) and v >= 0 for v in sample['segments'].values()), key
            assert math.isclose(sum(sample['segments'].values()), sample['total_ms'], rel_tol=1e-9, abs_tol=1e-9)
            assert math.isfinite(sample['call_ms']) and sample['call_ms'] > 0, key
    assert thread_keys == {(name, t, 'SIXEL -d none') for name in smeta['images']
                           for t in smeta['thread_budgets']}
print(f'validated {len(rows)} records for {len(images)} images, lossless pixels and retained-index generations')
