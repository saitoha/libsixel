#!/usr/bin/env python3
"""Explicit system transition controls using signed archives and private OS state.

This is shared preparation, not a timed TAP. Each real product invocation and
its file/receipt snapshots are retained independently of its exit assertion.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
p.add_argument('--alternate', type=Path, required=True)
p.add_argument('--upgrade', type=Path, required=True)
p.add_argument('--driver', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
here = Path(__file__).resolve().parent
results = []


def prepare(name):
    folder = a.output / name
    root = folder / 'root'
    (root / 'private/var/run').mkdir(parents=True)
    (folder / 'private-guard-marker').write_text('private transition only')
    env = dict(os.environ, QUICKLOOK_PRIVATE_ROOT=str(root),
               QUICKLOOK_PRIVATE_DRIVER=str(a.driver),
               QUICKLOOK_PRIVATE_INSTALLER=str(here / 'guard-installer.py'),
               QUICKLOOK_PRIVATE_PKGUTIL=str(here / 'guard-pkgutil.py'),
               QUICKLOOK_PRIVATE_OS=str(here / 'guard-pkgutil.py'))
    return folder, root, env


def snapshot(folder, root):
    files = {}
    for base in ['Applications', 'Library']:
        for path in sorted((root / base).rglob('*')):
            files[str(path.relative_to(root))] = dict(mode=path.lstat().st_mode,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None)
    receipt = folder / 'receipt.json'
    return dict(files=files, receipt=json.loads(receipt.read_text()) if receipt.exists() else None,
                registration='not provided by private Installer; no user mutator wired',
                defaults='not provided by private Installer; no user mutator wired')


def run(folder, root, env, label, action, package, expected=0):
    before = snapshot(folder, root)
    command = [str(a.driver), 'manage', action, str(package), '/unused', '', 'yes']
    start = time.monotonic()
    q = subprocess.run(command, env=env, capture_output=True, text=True)
    after = snapshot(folder, root)
    record = dict(name=label, command=command, exit=q.returncode,
                  seconds=time.monotonic()-start, stdout=q.stdout, stderr=q.stderr,
                  before=before, after=after)
    results.append(record)
    (a.output / 'transitions.json').write_text(json.dumps(results, indent=2))
    assert q.returncode == expected, record
    if expected != 0:
        assert before == after, label
    return after


f, root, env = prepare('versions')
run(f, root, env, 'fresh', 'install', a.package)
run(f, root, env, 'identical', 'install', a.package)
run(f, root, env, 'upgrade', 'install', a.upgrade)
run(f, root, env, 'remove-upgrade', 'uninstall', a.upgrade)
run(f, root, env, 'already-absent', 'uninstall', a.upgrade)
# Failed Installer operations intentionally preserve recovery locks. Negative
# scenarios use independent private roots instead of clearing those locks.
for name, installed, incoming in [('same-version-different', a.package, a.alternate),
                                  ('downgrade', a.upgrade, a.package)]:
    f, root, env = prepare(name)
    run(f, root, env, name + '-prepare', 'install', installed)
    run(f, root, env, name, 'install', incoming, 1)
for name in ['foreign', 'unknown-child', 'missing-receipt', 'receipt-only']:
    f, root, env = prepare(name)
    if name != 'foreign':
        run(f, root, env, name + '-prepare', 'install', a.package)
    if name in ['foreign', 'unknown-child']:
        path = root / 'Applications/SixelQuickLookHost.app/foreign'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('preserve this foreign descendant')
    if name == 'missing-receipt':
        (f / 'receipt.json').unlink()
    if name == 'receipt-only':
        # Only known fixture-owned trees are removed to produce this negative.
        import shutil
        shutil.rmtree(root / 'Applications')
        shutil.rmtree(root / 'Library')
    run(f, root, env, name + '-refusal',
        'install' if name == 'foreign' else 'uninstall', a.package, 1)
print(f'{len(results)} transition calls with independent file/receipt snapshots')
