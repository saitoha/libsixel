"""Capture transient guard facts around each private competing action."""
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import time

ROOTS = {'host': 'Applications/SixelQuickLookHost.app',
         'bridge': 'Applications/SixelPreviewBridge.app',
         'support': 'Library/Application Support/libsixel/QuickLook'}


def snapshot(root):
    payload = {}
    for name, relative in ROOTS.items():
        base = root / relative
        entries = {}
        if base.exists():
            for path in [base, *sorted(base.rglob('*'))]:
                mode = path.lstat().st_mode
                entries[str(path.relative_to(base))] = dict(mode=mode,
                    sha256=hashlib.sha256(path.read_bytes()).hexdigest()
                    if stat.S_ISREG(mode) else None)
        payload[name] = entries
    receipt = root.parent / 'receipt.json'
    return dict(payload=payload,
                receipt=receipt.read_text() if receipt.exists() else None,
                managedGuard=(root / 'private/var/run/libsixel-quicklook-managed.lock').exists(),
                packageLock=(root / 'private/var/run/libsixel-quicklook.lock').exists())


def observe(phase):
    root = Path(os.environ['QUICKLOOK_PRIVATE_ROOT'])
    tool = Path(os.environ['QUICKLOOK_PRIVATE_DRIVER'])
    package = os.environ['QL_PACKAGE']
    start = time.monotonic()
    env = dict(os.environ)
    env.pop('QL_PROBE', None)
    records = []
    for action in ['uninstall', 'install', 'installcheck']:
        before = snapshot(root)
        command_start = time.monotonic()
        command = [tool, 'manage', action, package, '/unused', '', 'yes']
        result = subprocess.run(command, capture_output=True, text=True, env=env)
        records.append(dict(action=action, status=result.returncode,
                            stderr=result.stderr, seconds=time.monotonic()-command_start,
                            before=before, after=snapshot(root)))
    record = dict(phase=phase, commands=records, seconds=time.monotonic()-start)
    path = root.parent / 'probes.json'
    rows = json.loads(path.read_text()) if path.exists() else []
    rows.append(record)
    path.write_text(json.dumps(rows, indent=2))
    # Shared mutation preparation retains raw evidence. Numbered observations
    # independently identify the action/window or residual invariant that fails.
    assert all(x['status'] == 1 for x in records), record
