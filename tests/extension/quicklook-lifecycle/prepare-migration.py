#!/usr/bin/env python3
"""Bounded product migration controls, with explicit synthetic OS responses.

Only private paths are changed. Signed old/new distributions remain separate
inputs. Registration response is an injected possibility, not real OS proof.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--old', type=Path, required=True)
p.add_argument('--new', type=Path, required=True)
p.add_argument('--driver', type=Path, required=True)
p.add_argument('--os-tool', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
results = []
paths = {'host': 'Applications/SixelQuickLookHost.app',
         'bridge': 'Applications/SixelPreviewBridge.app',
         'preview': 'Applications/SixelQuickLookHost.app/Contents/PlugIns/SixelPreview.appex',
         'thumbnail': 'Applications/SixelQuickLookHost.app/Contents/PlugIns/SixelThumbnail.appex'}
old = json.loads((a.old / 'payload-manifest.json').read_text())
new = json.loads((a.new / 'payload-manifest.json').read_text())
assert old['packageVersion'] != new['packageVersion']
libc = ctypes.CDLL('/usr/lib/libSystem.B.dylib', use_errno=True)
libc.listxattr.restype = ctypes.c_ssize_t
libc.getxattr.restype = ctypes.c_ssize_t


def write(path, value):
    path.write_text(json.dumps(value, indent=2))


def read(path):
    return json.loads(path.read_text())


def snapshot(base):
    files = {}
    for path in [base, *sorted(base.rglob('*'))]:
        raw = os.fsencode(path)
        count = libc.listxattr(raw, None, 0, 1)
        assert count >= 0
        names = ctypes.create_string_buffer(count)
        assert libc.listxattr(raw, names, count, 1) == count
        attrs = {}
        for name in filter(None, names.raw.split(b'\0')):
            size = libc.getxattr(raw, name, None, 0, 0, 1)
            assert size >= 0
            value = ctypes.create_string_buffer(size)
            assert libc.getxattr(raw, name, value, size, 0, 1) == size
            attrs[os.fsdecode(name)] = value.raw.hex()
        files[str(path.relative_to(base))] = dict(mode=path.lstat().st_mode,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None,
            xattrs=attrs)
    return files


def run(folder, args, journal, expected=0):
    command = [str(a.driver), *map(str, args)]
    env = dict(os.environ, QUICKLOOK_PRIVATE_ROOT=str(folder / 'root'),
        QUICKLOOK_PRIVATE_JOURNAL=str(journal), QUICKLOOK_PRIVATE_OS=str(a.os_tool))
    start = time.monotonic()
    q = subprocess.run(command, env=env, text=True, capture_output=True)
    record = dict(scenario=folder.name, command=command, exit=q.returncode,
        seconds=time.monotonic()-start, stdout=q.stdout, stderr=q.stderr,
        load=os.getloadavg(), expectationMet=q.returncode == expected)
    results.append(record)
    write(a.output / 'operations.json', results)
    write(folder / 'last-operation.json', record)
    assert record['expectationMet'], record
    return record


def identity(base, name, manifest):
    path = str(base / paths[name])
    return dict(path=path, canonicalPath=path,
        identifier='com.saitoha.libsixel.quicklook.'+name,
        version=manifest['packageVersion'],
        signature=next(c['signature'] for c in manifest['codes'] if c['path'] == paths[name]))


for case in ['normal', 'later-default', 'later-provider', 'wrong-archive',
             'occupied', 'wrong-final', 'wrong-session', 'archive-check-failure']:
    folder = a.output / case
    root = folder / 'root'
    (root / 'private/var/run').mkdir(parents=True)
    for part in ['Applications', 'Library']:
        shutil.copytree(a.new / 'work/expanded/payload.pkg/Payload' / part, root / part)
    legacy = root / 'Legacy'
    (legacy / 'Applications').mkdir(parents=True)
    journal = folder / 'journal'
    journal.mkdir(mode=0o700)
    applications = {}
    oldapps = {}
    newapps = {}
    providers = {}
    oldproviders = {}
    for name in paths:
        oldapps[name] = identity(legacy, name, old)
        newapps[name] = identity(root, name, new)
        applications[oldapps[name]['path']] = oldapps[name]
        applications[newapps[name]['path']] = newapps[name]
    for index, name in enumerate(['preview', 'thumbnail'], 1):
        for records, apps in [(providers, newapps), (oldproviders, oldapps)]:
            records[name] = {k:v for k,v in apps[name].items() if k != 'signature'}
            records[name].update(uuid=f'11111111-2222-3333-4444-{index:012d}', election='+')
    original = dict(uti='com.saitoha.libsixel.sixel', handler=oldapps['bridge']['identifier'],
                    application=oldapps['bridge'])
    system_default = dict(original, application=newapps['bridge'])
    state = dict(session=dict(uid=os.getuid(), euid=os.getuid(), consoleUID=os.getuid(),
        session=12345, gui=True, remote=False), applications=applications,
        providerRecords=providers, providerRecordsByPath={r['path']:r for r in [*providers.values(), *oldproviders.values()]},
        plugins={r['identifier']:[r] for r in oldproviders.values()},
        ls={oldapps[n]['identifier']:[oldapps[n]] for n in ['host', 'bridge']},
        six=original, sixel=original, calls=[],
        registrationDefaults={newapps['bridge']['path']:system_default,
                              oldapps['bridge']['path']:original},
        receipt={'pkgid':'com.saitoha.libsixel.quicklook.pkg',
            'pkg-version':new['packageVersion'], 'volume':'/', 'install-location':''})
    write(root / 'live.json', state)
    write(folder / 'before.json', state)
    request = dict(uid=os.getuid(), session=12345, legacy=[])
    for name in ['host', 'bridge']:
        path = legacy / paths[name]
        shutil.copytree(a.old / 'work/expanded/payload.pkg/Payload' / paths[name], path)
        assert libc.setxattr(os.fsencode(path), b'org.libsixel.root', b'preserved', 9, 0, 0) == 0
        sub = folder / name
        sub.mkdir()
        (sub / 'journal').mkdir(mode=0o700)
        write(sub / 'request.json', dict(uid=os.getuid(), session=12345, component=name, path=str(path)))
        write(sub / 'original-copy.json', dict(path=str(path), files=snapshot(path), signature=oldapps[name]['signature']))
        request['legacy'].append(dict(distribution=str(a.old), request=str(sub / 'request.json'), journal=str(sub / 'journal')))
    write(folder / 'request.json', request)
    if case == 'archive-check-failure':
        sub = folder / 'bridge'
        (sub / 'journal/archive-check').mkdir()
        write(folder/'before-target.json',read(root/'live.json'))
        run(folder, ['legacy','archive',a.old,sub/'request.json',sub/'journal'],sub/'journal',1)
        assert Path(oldapps['bridge']['path']).exists()
        assert read(sub/'journal/journal.json')['state'] == 'archive-pending'
        run(folder, ['legacy','remove',a.old,sub/'request.json',sub/'journal'],sub/'journal',1)
        continue
    for operation in ['archive', 'remove']:
        for name in ['host', 'bridge']:
            sub = folder / name
            run(folder,['legacy',operation,a.old,sub/'request.json',sub/'journal'],sub/'journal')
    if case == 'wrong-archive':
        with (folder / 'bridge/journal/copy.zip').open('ab') as f:f.write(b'changed')
    if case == 'wrong-session':
        request['session'] += 1
        write(folder/'request.json',request)
    write(folder/'before-target.json',read(root/'live.json'))
    run(folder,['migration','activate',a.new,folder/'request.json',journal],journal,
        1 if case in ['wrong-archive','wrong-session'] else 0)
    if case in ['wrong-archive','wrong-session']:
        continue
    live = read(root/'live.json')
    if case == 'later-default':
        live['six']['handler'] = 'org.example.later'
        live['sixel']['handler'] = 'org.example.later'
    if case == 'later-provider':
        live['plugins']['com.saitoha.libsixel.quicklook.preview'][0]['election'] = '-'
    write(root/'live.json',live)
    write(folder/'before-prepare.json',live)
    write(folder/'before-target.json',live)
    run(folder,['migration','prepare',a.new,folder/'request.json',journal],journal,
        1 if case in ['later-default','later-provider'] else 0)
    if case in ['later-default','later-provider']:
        assert read(root/'live.json') == live
        continue
    write(folder/'ready.json',read(journal/'journal.json'))
    write(folder/'participants.json',dict(format=1,participants=[dict(uid=os.getuid(),session=12345,
        migration=True,journal=str(journal/'journal.json'),
        journalSHA256=hashlib.sha256((journal/'journal.json').read_bytes()).hexdigest())]))
    run(folder,['remove-users',a.new,folder/'participants.json'],journal)
    live = read(root/'live.json')
    assert 'receipt' not in live
    if case == 'occupied':
        occupied=Path(oldapps['bridge']['path']);occupied.mkdir()
        (occupied/'foreign').write_text('preserve')
    if case == 'wrong-final':
        live['registrationDefaults'].pop(oldapps['bridge']['path'])
        write(root/'live.json',live)
    write(folder/'before-target.json',read(root/'live.json'))
    run(folder,['migration','restore',a.new,folder/'request.json',journal],journal,
        1 if case == 'occupied' else 0)
    if case == 'occupied':
        assert (occupied/'foreign').read_text() == 'preserve'
        continue
    run(folder,['migration','verify',a.new,folder/'request.json',journal],journal,
        1 if case == 'wrong-final' else 0)
    if case == 'normal':
        live = read(root/'live.json')
        assert live['six'] == original and live['sixel'] == original
        assert 'default' not in live['calls']
        for name in ['host','bridge']:
            assert snapshot(Path(oldapps[name]['path'])) == read(folder/name/'original-copy.json')['files']
print('Bounded no-setter product migration controls retained; all OS responses are private')
