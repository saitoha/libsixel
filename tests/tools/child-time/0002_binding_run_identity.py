#!/usr/bin/env python3
"""Sibling measured tests retain one binding environment preparation identity."""
# Policy: docs/testing/guide.md
# Policy: docs/build/test-harness.md
import os
from pathlib import Path
import subprocess
import sys


def check(executor):
    if os.environ.get('CROSS_COMPILING') == 'yes' or not executor:
        return 'a native child-time executor is required'
    if os.name != 'posix':
        return 'POSIX child-time supervision is unavailable'
    env = dict(os.environ, MESON_BUILD_ROOT=os.getcwd())
    env.pop('MESON_TEST_RUN_ID', None)
    command = [executor, '--record-child-time', 'binding-run-identity', '0',
               '--', sys.executable, '-c',
               'import os; print(os.getenv("MESON_TEST_RUN_ID", "missing"))']
    # Different executors and guardians must export the same original parent.
    for expected in (str(os.getpid()), str(os.getpid()), 'explicit-run'):
        if expected == 'explicit-run':
            env['MESON_TEST_RUN_ID'] = expected
        result = subprocess.run(command, env=env, stdin=subprocess.DEVNULL,
                                capture_output=True, text=True, timeout=5)
        if 'child-time unsupported' in result.stderr:
            return 'complete-command timing is unavailable'
        if result.returncode or result.stdout.strip() != expected:
            raise RuntimeError('binding run identity changed: ' + repr(result))
    return None


try:
    executor = sys.argv[1] if len(sys.argv) > 1 else os.environ.get('SIXEL_TEST_CHILD_TIME')
    skip = check(str(Path(executor).resolve()) if executor else None)
except (OSError, RuntimeError, subprocess.SubprocessError) as error:
    print('1..1\nnot ok 1 - measured siblings share binding run identity')
    print('# ' + str(error), file=sys.stderr)
    sys.exit(1)
print('1..0 # SKIP ' + skip if skip else
      '1..1\nok 1 - measured siblings share binding run identity')
