#!/usr/bin/env python3
"""Validate the published matrix, lossless contracts and fixed-stream scaling."""
import hashlib
import json
import math
from pathlib import Path
import sys

root = Path(sys.argv[1])
p = json.loads((root / 'results.json').read_text())
rows = p['rows']
n = p['metadata']['generations']
assert len(rows) == 3 * (11 * 4 + 6 * n)
keys = set()
for row in rows:
    key = (row['image'], row['experiment'], row['codec'], row['threads'], row.get('mode'), row.get('generation'))
    assert key not in keys, key
    keys.add(key)
    for field in ['encode_ms', 'decode_ms']:
        assert len(row[field]) == p['metadata']['runs']
        assert all(math.isfinite(x) and x > 0 for x in row[field])
    assert 0 <= row['ms_ssim'] <= 1.000001
    assert math.isfinite(row['delta_e']) and row['delta_e'] >= 0
    assert row['bytes'] > 0
    assert row['transport_bytes'] == (row['bytes'] if row['codec'].startswith('SIXEL') else 4 * ((row['bytes'] + 2) // 3))
    if row['codec'] in ['PNG', 'WebP lossless']:
        assert row['exact_input']
        assert row['ms_ssim'] == 1
    if row['experiment'] == 'generation':
        palette = row['codec'] in ['SIXEL default', 'GIF']
        assert row['mode'] == ('indexed' if palette else 'RGB')
        if palette:
            assert row['exact_generation_one']
for name, digest in p['metadata']['inputs'].items():
    assert hashlib.sha256((root / f'{name}.png').read_bytes()).hexdigest() == digest
    for codec in {r['codec'] for r in rows if r['experiment'] == 'scaling'}:
        group = [r for r in rows if r['image'] == name and r['codec'] == codec and r['experiment'] == 'scaling']
        assert sorted(r['threads'] for r in group) == [1,2,4,8]
        assert len({r['decoder_stream_sha256'] for r in group}) == 1
print(f'validated {len(rows)} records, lossless pixels and retained-index generations')
