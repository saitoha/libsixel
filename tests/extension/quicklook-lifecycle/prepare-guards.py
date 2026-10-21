#!/usr/bin/env python3
"""Explicit disposable Installer boundary controls; no system Installer call."""
import argparse
from pathlib import Path
import os
import json
import subprocess
import time
p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
p.add_argument('--driver', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(exist_ok=False, parents=True)
here = Path(__file__).resolve().parent
results = []


def prepare(name):
    f = a.output / name
    root = f / 'root'
    (root / 'private/var/run').mkdir(parents=True)
    (f / 'private-guard-marker').write_text('private boundary only')
    env = dict(os.environ, QUICKLOOK_PRIVATE_ROOT=str(root),
               QUICKLOOK_PRIVATE_DRIVER=str(a.driver),
               QUICKLOOK_PRIVATE_INSTALLER=str(here / 'guard-installer.py'),
               QUICKLOOK_PRIVATE_PKGUTIL=str(here / 'guard-pkgutil.py'),
               QUICKLOOK_PRIVATE_OS=str(here / 'guard-pkgutil.py'),
               QL_PACKAGE=str(a.package))
    return f, root, env


def run(name, env, args, status=0):
    command = [str(a.driver), *map(str, args)]
    start = time.monotonic()
    q = subprocess.run(command, env=env, text=True, capture_output=True)
    record = dict(name=name, command=command, exit=q.returncode,
                  seconds=time.monotonic() - start, stdout=q.stdout, stderr=q.stderr)
    results.append(record)
    (a.output / 'commands.json').write_text(json.dumps(results, indent=2))
    assert q.returncode == status, record


def manage(name, env, action, status=0):
    run(name, env, ['manage', action, a.package, '/unused', '', 'yes'], status)


f, root, env = prepare('managed')
manage('initial', env, 'install')
manage('reinstall', dict(env, QL_PROBE='1'), 'install')
probes = json.loads((f / 'probes.json').read_text())
assert len(probes) == 3
assert not (root / 'private/var/run/libsixel-quicklook-managed.lock').exists()
manage('final-readonly', env, 'installcheck')
# Persist the three independent windows for the numbered observations.
for i, probe in enumerate(probes, 1):
    (a.output / f'guard-{i}.json').write_text(json.dumps(probe, indent=2))
f, root, env = prepare('standalone')
for i in range(2):
    start = time.monotonic()
    q = subprocess.run([str(here / 'guard-installer.py'), '-pkg',
                        str(a.package / 'SixelQuickLook.pkg'), '-target', '/'],
                       env=env, text=True, capture_output=True)
    results.append(dict(name=f'standalone-{i}', command=q.args,
                        exit=q.returncode, seconds=time.monotonic()-start,
                        stdout=q.stdout, stderr=q.stderr))
    (a.output / 'commands.json').write_text(json.dumps(results, indent=2))
    assert q.returncode == 0, q.stderr
    manage(f'standalone-check-{i}', env, 'installcheck')
    assert not list((root / 'private/var/run').iterdir())
manage('managed-after-standalone', env, 'install')
manage('remove', env, 'uninstall')
assert not (f / 'receipt.json').exists()
owned_paths = ['Applications/SixelQuickLookHost.app',
               'Applications/SixelPreviewBridge.app',
               'Library/Application Support/libsixel/QuickLook']
payload_absent = {path: not os.path.lexists(root / path) for path in owned_paths}
assert all(payload_absent.values())
(a.output / 'standalone.json').write_text(json.dumps(dict(
    repeated=2, manualCompletion=False, locks=list((root / 'private/var/run').iterdir()),
    receiptAbsent=True, payloadAbsent=payload_absent)))
for name, phase, inner in [('before-pre', 'QL_BEFORE_PRE', False),
                          ('before-post', 'QL_BEFORE_POST', True),
                          ('after-post', 'QL_AFTER_POST', False)]:
    f, root, env = prepare(name)
    manage(name, dict(env, **{phase: '37'}), 'install', 37)
    assert (root / 'private/var/run/libsixel-quicklook-managed.lock').exists()
    assert (root / 'private/var/run/libsixel-quicklook.lock').exists() == inner
    manage(name + '-uninstall-refused', env, 'uninstall', 1)
    manage(name + '-observer-refused', env, 'installcheck', 1)
print('Three guard windows, standalone reuse/removal, and three failed stages verified')
