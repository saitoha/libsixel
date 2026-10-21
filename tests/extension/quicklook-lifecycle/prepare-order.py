#!/usr/bin/env python3
"""Bounded private orchestration controls, separately timed from assertions."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
p = argparse.ArgumentParser()
p.add_argument('--tool', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--only', choices=['normal', 'two-errors', 'installer-error',
                                  'command-error', 'record-error'])
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
for name, failures, expected, calls in [
    ('normal', {}, 0, ['check', 'install', 'activate', 'set-default', 'observe', 'deactivate', 'remove', 'restore', 'verify-restored']),
    ('two-errors', {'observe': 23, 'deactivate': 41}, 23, ['check', 'install', 'activate', 'set-default', 'observe', 'deactivate']),
    ('installer-error', {'install': 37}, 37, ['check', 'install']),
    ('command-error', {'activate': 37}, 37, ['check', 'install', 'activate', 'deactivate', 'remove', 'restore', 'verify-restored']),
    ('record-error', {'activate': 37, 'recordFailure': 'activate'}, 37, ['check', 'install', 'activate'])]:
    if a.only and name != a.only:
        continue
    f = a.output / name
    f.mkdir()
    journal = f / 'record'
    journal.mkdir(mode=0o700)
    (f / 'stage-status.json').write_text(json.dumps(failures))
    plan = dict(format=1, explicitLifecycle=True)
    for stage in ['check', 'install', 'activate', 'set-default', 'observe', 'deactivate', 'remove', 'restore', 'verify-restored']:
        plan[stage] = [sys.executable, str(Path(__file__).with_name('lifecycle-stage.py')), str(f), stage]
    (f / 'plan.json').write_text(json.dumps(plan))
    command = [str(a.tool), 'lifecycle', str(f / 'plan.json'), str(journal)]
    start = time.monotonic()
    q = subprocess.run(command, text=True, capture_output=True)
    (f / 'result.json').write_text(json.dumps(dict(command=command, exit=q.returncode,
        seconds=time.monotonic()-start, stdout=q.stdout, stderr=q.stderr), indent=2))
    assert q.returncode == expected, q.stderr
    assert json.loads((f / 'stage-calls.json').read_text()) == calls
    result = json.loads((journal / 'journal.json').read_text())
    if name == 'record-error':
        assert result['state'] == 'running'
        assert result['stages'][-1]['state'] == 'pending'
        assert 'originalStatus' not in result and 'cleanupStatus' not in result
        assert 'originalStatus=37 cleanupStatus=null cleanupState=not-attempted recordingFailure=yes' in q.stderr
        assert (journal / 'journal.pending').read_text() == 'private persistence conflict'
    else:
        assert result['originalStatus'] == (expected if name != 'normal' else 0)
        assert result['cleanupStatus'] == (None if name == 'installer-error' else 41 if name == 'two-errors' else 0)
        assert result['cleanupState'] == ('not-attempted' if name == 'installer-error' else 'failed' if name == 'two-errors' else 'completed')
print('Selected explicit lifecycle order/error controls passed')
