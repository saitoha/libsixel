#!/usr/bin/env python3
"""Explicit private lifecycle preparation; never invoked by an installed test.

Build/compiler work precedes this command. Record each operation separately;
numbered observations inspect its live residual state and independent records.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
p.add_argument('--driver', type=Path, required=True)
p.add_argument('--os-tool', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--reactivation-only', action='store_true')
p.add_argument('--expect-vulnerable', action='store_true')
a = p.parse_args()
assert not a.expect_vulnerable or a.reactivation_only
a.output.mkdir(parents=True, exist_ok=False)
manifest = json.loads((a.package / 'payload-manifest.json').read_text())
payload = a.package / 'work/expanded/payload.pkg/Payload'
paths = {'host': 'Applications/SixelQuickLookHost.app',
         'bridge': 'Applications/SixelPreviewBridge.app',
         'preview': 'Applications/SixelQuickLookHost.app/Contents/PlugIns/SixelPreview.appex',
         'thumbnail': 'Applications/SixelQuickLookHost.app/Contents/PlugIns/SixelThumbnail.appex'}
results = []


def create(name):
    folder = a.output / name
    folder.mkdir()
    root = folder / 'root'
    root.mkdir()
    (root / 'private/var/run').mkdir(parents=True)
    for part in ['Applications', 'Library']:
        shutil.copytree(payload / part, root / part)
    journal = folder / 'journal'
    journal.mkdir(mode=0o700)
    apps = {}
    records = {}
    for kind, relative in paths.items():
        path = str(root / relative)
        sig = next(c['signature'] for c in manifest['codes'] if c['path'] == relative)
        record = dict(path=path, canonicalPath=path,
                      identifier='com.saitoha.libsixel.quicklook.' + kind,
                      version=manifest['packageVersion'], signature=sig)
        apps[path] = record
        records[kind] = record
    providers = {}
    for i, kind in enumerate(['preview', 'thumbnail'], 1):
        rec = {k: v for k, v in records[kind].items() if k != 'signature'}
        rec.update(uuid=f'11111111-2222-3333-4444-{i:012d}', election='+')
        providers[kind] = rec
    default = dict(uti='com.saitoha.libsixel.sixel', handler=records['host']['identifier'],
                   application=records['host'])
    state = dict(session=dict(uid=os.getuid(), euid=os.getuid(), consoleUID=os.getuid(),
                              session=12345, gui=True, remote=False),
                 applications=apps, providerRecords=providers, plugins={}, calls=[],
                 ls={records['host']['identifier']: [records['host']]},
                 six=default, sixel=default,
                 receipt={'pkgid': 'com.saitoha.libsixel.quicklook.pkg',
                          'pkg-version': manifest['packageVersion'],
                          'volume': '/', 'install-location': ''})
    write(root / 'live.json', state)
    write(folder / 'before.json', state)
    request = dict(uid=os.getuid(), session=12345, extensions=['six', 'sixel'],
                   restoreDefault=default)
    write(folder / 'request.json', request)
    return folder, root, journal, state


def write(path, value):
    path.write_text(json.dumps(value, indent=2))


def run(folder, action, expected=0):
    root, journal = folder / 'root', folder / 'journal'
    env = dict(os.environ, QUICKLOOK_PRIVATE_ROOT=str(root),
               QUICKLOOK_PRIVATE_OS=str(a.os_tool), QUICKLOOK_PRIVATE_JOURNAL=str(journal))
    command = [str(a.driver), 'user', action, str(a.package),
               str(folder / 'request.json'), str(journal)]
    start = time.monotonic()
    completed = subprocess.run(command, env=env, capture_output=True, text=True)
    record = dict(scenario=folder.name, action=action, command=command,
                  exit=completed.returncode, seconds=time.monotonic() - start,
                  stdout=completed.stdout, stderr=completed.stderr,
                  expectationMet=(completed.returncode == expected if expected == 0
                                  else completed.returncode != 0))
    results.append(record)
    write(a.output / 'operations.json', results)
    if not record['expectationMet']:
        raise AssertionError(record)
    return record


# Repeated observation cannot acquire ownership of an external later choice.
# The old-source mode is a negative control: the independent auditor must fail.
for case in ['reactivate-default', 'reactivate-provider']:
    f, root, journal, state = create(case)
    run(f, 'activate')
    if case == 'reactivate-default':
        run(f, 'set-default')
    live = json.loads((root / 'live.json').read_text())
    if case == 'reactivate-default':
        # Synthetic external query identity: no real application is registered.
        later = json.loads(json.dumps(state['six']))
        later['handler'] = 'org.example.explicit-later-choice'
        later['application'].update(identifier=later['handler'],
            path=str(root / 'ExternalChoice.app'),
            canonicalPath=str(root / 'ExternalChoice.app'))
        live['six'] = later
        live['sixel'] = later
    else:
        live['plugins']['com.saitoha.libsixel.quicklook.preview'][0]['election'] = '-'
    write(root / 'live.json', live)
    write(f / 'before-reactivation.json', live)
    before_journal = json.loads((journal / 'journal.json').read_text())
    write(f / 'journal-before-reactivation.json', before_journal)
    run(f, 'activate', expected=0 if a.expect_vulnerable else 1)
    after_journal = json.loads((journal / 'journal.json').read_text())
    write(f / 'journal-after-reactivation.json', after_journal)
    after = json.loads((root / 'live.json').read_text())
    write(f / 'after-reactivation.json', after)
    if not a.expect_vulnerable:
        assert before_journal['before'] == after_journal['before']
        assert before_journal['owned'] == after_journal['owned']
        assert live == after
    run(f, 'deactivate', expected=0 if case == 'reactivate-default' or a.expect_vulnerable else 1)
    after = json.loads((root / 'live.json').read_text())
    if not a.expect_vulnerable:
        if case == 'reactivate-default':
            assert after['six'] == later and after['sixel'] == later
            assert after['calls'].count('default') == 1
        else:
            assert after['plugins']['com.saitoha.libsixel.quicklook.preview'][0]['election'] == '-'
            assert '-r' not in after['calls']
if a.reactivation_only:
    print('Two independent repeated-activation sequences recorded')
    sys.exit(0)


# A normal explicit sequence: activation must not invoke the default setter.
f, root, journal, state = create('normal')
original = state['six']
run(f, 'activate')
(f / 'journal-before-retry.json').write_bytes((journal / 'journal.json').read_bytes())
run(f, 'activate')
(f / 'journal-after-retry.json').write_bytes((journal / 'journal.json').read_bytes())
live = json.loads((root / 'live.json').read_text())
assert live['six'] == original and 'default' not in live['calls']
write(f / 'after-activation.json', live)
run(f, 'set-default')
write(f / 'after-default.json', json.loads((root / 'live.json').read_text()))
run(f, 'deactivate')
live = json.loads((root / 'live.json').read_text())
assert live['six'] == original and all(not v for v in live['plugins'].values())
write(f / 'after-deactivation.json', live)

for case in ['wrong-user', 'wrong-session', 'missing-user', 'root-user',
             'no-gui', 'duplicate', 'old-path', 'wrong-version', 'changed-payload',
             'register-failure', 'provider-failure']:
    f, root, journal, state = create(case)
    if case == 'wrong-user':
        state['session']['uid'] += 1
    if case == 'wrong-session':
        state['session']['session'] += 1
    if case in ['missing-user', 'root-user']:
        request = json.loads((f / 'request.json').read_text())
        if case == 'missing-user':
            request.pop('uid')
        else:
            request['uid'] = 0
        write(f / 'request.json', request)
    if case == 'old-path':
        state['ls']['com.saitoha.libsixel.quicklook.host'][0]['path'] += '.old'
    if case == 'wrong-version':
        state['ls']['com.saitoha.libsixel.quicklook.host'][0]['version'] = '0.0.1'
    if case == 'no-gui':
        state['session']['gui'] = False
    if case == 'duplicate':
        state['ls']['com.saitoha.libsixel.quicklook.host'] *= 2
    if case == 'changed-payload':
        (root / paths['bridge'] / 'unexpected').write_text('foreign')
    if case == 'register-failure':
        state['fail'] = 'register'
    if case == 'provider-failure':
        state['fail'] = '-a'
    write(root / 'live.json', state)
    run(f, 'activate', expected=1)
    live = json.loads((root / 'live.json').read_text())
    if case not in ['register-failure', 'provider-failure']:
        assert not live['calls']
    assert live['six'] == state['six']

for case in ['default-failure', 'default-wide-status', 'default-readback', 'missing-original',
             'later-choice', 'provider-changed', 'journal-pending',
             'shared-uti', 'ambiguous-original', 'restore-original-missing']:
    f, root, journal, state = create(case)
    run(f, 'activate')
    live = json.loads((root / 'live.json').read_text())
    if case in ['default-failure', 'default-wide-status', 'default-readback']:
        live['fail'] = 'default' if case == 'default-failure' else case
        write(root / 'live.json', live)
        run(f, 'set-default', expected=1)
    elif case == 'missing-original':
        live['ls']['com.saitoha.libsixel.quicklook.host'] = []
        write(root / 'live.json', live)
        run(f, 'set-default', expected=1)
        assert 'default' not in json.loads((root / 'live.json').read_text())['calls']
    elif case in ['shared-uti', 'ambiguous-original']:
        if case == 'shared-uti':
            live['six']['uti'] = 'public.data'
            live['sixel']['uti'] = 'public.data'
            request = json.loads((f / 'request.json').read_text())
            request['restoreDefault'] = live['six']
            write(f / 'request.json', request)
        else:
            live['ls']['com.saitoha.libsixel.quicklook.host'] *= 2
        write(root / 'live.json', live)
        run(f, 'set-default', expected=1)
        assert 'default' not in json.loads((root / 'live.json').read_text())['calls']
    elif case == 'restore-original-missing':
        run(f, 'set-default')
        live = json.loads((root / 'live.json').read_text())
        live['ls']['com.saitoha.libsixel.quicklook.host'] = []
        write(root / 'live.json', live)
        run(f, 'deactivate', expected=1)
        after = json.loads((root / 'live.json').read_text())
        assert after['calls'].count('default') == 1 and '-r' not in after['calls']
    elif case == 'later-choice':
        run(f, 'set-default')
        live = json.loads((root / 'live.json').read_text())
        # A later explicit user choice is distinct from this operation's owned value.
        live['six'] = state['six']
        live['sixel'] = state['sixel']
        calls = list(live['calls'])
        write(root / 'live.json', live)
        run(f, 'activate', expected=1)
        run(f, 'deactivate')
        after = json.loads((root / 'live.json').read_text())
        assert after['six'] == state['six']
        assert after['calls'].count('default') == calls.count('default')
    elif case == 'provider-changed':
        live['plugins']['com.saitoha.libsixel.quicklook.preview'][0]['election'] = '-'
        write(root / 'live.json', live)
        run(f, 'deactivate', expected=1)
        assert '-r' not in json.loads((root / 'live.json').read_text())['calls']
    else:
        j = json.loads((journal / 'journal.json').read_text())
        j['state'] = 'pending'
        write(journal / 'journal.json', j)
        os.chmod(journal / 'journal.json', 0o600)
        run(f, 'deactivate', expected=1)
        assert '-r' not in json.loads((root / 'live.json').read_text())['calls']
write(a.output / 'setup.json', dict(package=str(a.package),
      manifestSHA256=hashlib.sha256((a.package / 'payload-manifest.json').read_bytes()).hexdigest(),
      driver=str(a.driver), driverSHA256=hashlib.sha256(a.driver.read_bytes()).hexdigest(),
      boundary='private OS and GUI-session replacements; actual signed payload and journal implementation'))
print(f'{len(results)} explicitly executed private operations; all expectations met')

def copy_xattrs(path):
    libc = ctypes.CDLL('/usr/lib/libSystem.B.dylib', use_errno=True)
    libc.listxattr.restype = ctypes.c_ssize_t
    libc.getxattr.restype = ctypes.c_ssize_t
    raw = os.fsencode(path)
    count = libc.listxattr(raw, None, 0, 1)
    assert count >= 0
    names = ctypes.create_string_buffer(count)
    assert libc.listxattr(raw, names, count, 1) == count
    attrs = {}
    for name in names.raw.split(b'\0'):
        if not name:
            continue
        size = libc.getxattr(raw, name, None, 0, 0, 1)
        assert size >= 0
        value = ctypes.create_string_buffer(size)
        assert libc.getxattr(raw, name, value, size, 0, 1) == size
        attrs[os.fsdecode(name)] = value.raw.hex()
    return attrs


def copy_state(base):
    result = {}
    for path in [base, *sorted(base.rglob('*'))]:
        result[str(path.relative_to(base))] = dict(
            mode=path.lstat().st_mode,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None,
            xattrs=copy_xattrs(path))
    return result

# Legacy bytes are from the independently supplied original distribution.
f, root, journal, state = create('legacy')
legacy = root / 'Legacy' / 'SixelPreviewBridge.app'
legacy.parent.mkdir()
shutil.copytree(root / paths['bridge'], legacy)
# A nonempty attribute detects a restore that silently drops metadata.
libc = ctypes.CDLL('/usr/lib/libSystem.B.dylib', use_errno=True)
assert libc.setxattr(os.fsencode(legacy / 'Contents/Info.plist'),
                     b'org.libsixel.preservation',
                     b'original', 8, 0, 0) == 0
assert libc.setxattr(os.fsencode(legacy), b'org.libsixel.root',
                     b'preserved', 9, 0, 0) == 0
original_copy = copy_state(legacy)
write(f / 'original-copy.json', dict(path=str(legacy), files=original_copy,
      signature=next(c['signature'] for c in manifest['codes']
                     if c['path'] == paths['bridge'])))
record = dict(state['applications'][str(root / paths['bridge'])])
record.update(path=str(legacy), canonicalPath=str(legacy))
state['applications'][str(legacy)] = record
write(root / 'live.json', state)
request = dict(uid=os.getuid(), session=12345, component='bridge', path=str(legacy))
write(f / 'request.json', request)
for operation in ['archive', 'remove', 'restore']:
    env = dict(os.environ, QUICKLOOK_PRIVATE_ROOT=str(root),
               QUICKLOOK_PRIVATE_OS=str(a.os_tool), QUICKLOOK_PRIVATE_JOURNAL=str(journal))
    command = [str(a.driver), 'legacy', operation, str(a.package),
               str(f / 'request.json'), str(journal)]
    start = time.monotonic()
    q = subprocess.run(command, env=env, text=True, capture_output=True)
    result = dict(scenario='legacy', action=operation, command=command,
                  exit=q.returncode, seconds=time.monotonic()-start,
                  stdout=q.stdout, stderr=q.stderr)
    results.append(result)
    write(a.output / 'operations.json', results)
    assert q.returncode == 0, result
    if operation == 'archive':
        foreign = legacy / 'foreign-user-data'
        foreign.write_text('preserve')
        start = time.monotonic()
        trial = subprocess.run([str(a.driver), 'legacy', 'remove', str(a.package),
            str(f / 'request.json'), str(journal)], env=env, text=True, capture_output=True)
        assert trial.returncode != 0 and foreign.read_text() == 'preserve'
        write(f / 'foreign-preserved.json', dict(exit=trial.returncode,
              command=trial.args, seconds=time.monotonic()-start,
              stdout=trial.stdout, stderr=trial.stderr, content=foreign.read_text()))
        foreign.unlink()
    if operation == 'remove':
        assert not legacy.exists()
        legacy.mkdir()
        (legacy / 'new-user-data').write_text('do not replace')
        start = time.monotonic()
        trial = subprocess.run([str(a.driver), 'legacy', 'restore', str(a.package),
            str(f / 'request.json'), str(journal)], env=env, text=True, capture_output=True)
        assert trial.returncode != 0 and (legacy / 'new-user-data').read_text() == 'do not replace'
        write(f / 'occupied-preserved.json', dict(exit=trial.returncode,
              command=trial.args, seconds=time.monotonic()-start,
              stdout=trial.stdout, stderr=trial.stderr, content=(legacy / 'new-user-data').read_text()))
        (legacy / 'new-user-data').unlink()
        legacy.rmdir()
assert legacy.exists() and (journal / 'copy.zip').exists()
# Independent byte/mode/xattr verification of the restored copy. This does
# not trust the product journal's success state or its inventory helper.

restored = copy_state(legacy)
expected_copy = original_copy
write(f / 'independent-restored-copy.json', dict(expected=expected_copy, actual=restored))
assert restored == expected_copy
print('Legacy restore completed; independent pre-removal metadata retained for measured observations')

# User evidence is consumed by root removal; a declaration flag is insufficient.
f, root, journal, state = create('participants')
run(f, 'activate')
participants = dict(format=1, participants=[dict(uid=os.getuid(),
                    journal=str(journal / 'journal.json'))])
write(f / 'participants.json', participants)
env = dict(os.environ, QUICKLOOK_PRIVATE_ROOT=str(root),
           QUICKLOOK_PRIVATE_OS=str(a.os_tool), QUICKLOOK_PRIVATE_JOURNAL=str(journal))
for phase, expected in [('active-refused', 1), ('deactivated-remove', 0)]:
    if expected == 0:
        run(f, 'deactivate')
    command = [str(a.driver), 'remove-users', str(a.package), str(f / 'participants.json')]
    start = time.monotonic()
    q = subprocess.run(command, env=env, text=True, capture_output=True)
    results.append(dict(scenario='participants', action=phase, command=command,
        seconds=time.monotonic()-start, exit=q.returncode, stdout=q.stdout, stderr=q.stderr))
    write(a.output / 'operations.json', results)
    assert q.returncode == expected, results[-1]
    assert (root / paths['host']).exists() == (expected != 0)
    after = json.loads((root / 'live.json').read_text())
    assert ('receipt' in after) == (expected != 0)
    assert after['six'] == state['six']
print('Pending participant refuses removal; completed evidence permits owned removal')
