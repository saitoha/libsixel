#!/usr/bin/env python3
"""A delayed stderr reader must not discard successful child-time evidence."""
# Policy: docs/testing/guide.md
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import time


def check(executor):
    if os.environ.get('CROSS_COMPILING') == 'yes':
        return 'child-time supervision requires a native build'
    if not executor:
        return 'the native child-time executor is disabled'
    if os.name != 'posix':
        return 'POSIX pipe supervision is unavailable'
    import fcntl

    probe = subprocess.run(
        [executor, '--record-child-time', 'probe', '0', '--',
         sys.executable, '-c', 'pass'], stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    if b'child-time unsupported' in probe.stderr:
        return 'complete-command timing is unavailable'
    if probe.returncode:
        raise RuntimeError('executor capability probe failed: ' +
                           repr(probe.stderr))

    read_fd, write_fd = os.pipe()
    process = None
    try:
        # Fill the destination before spawning. This isolates marker delivery
        # from command-output forwarding and works with each OS pipe capacity.
        fcntl.fcntl(write_fd, fcntl.F_SETFL, os.O_NONBLOCK)
        filled = 0
        while True:
            try:
                filled += os.write(write_fd, b'x' * 4096)
            except BlockingIOError:
                break
        process = subprocess.Popen(
            [executor, '--record-child-time', 'marker-backpressure', '0', '--',
             sys.executable, '-c',
             'import os; os.write(1,b"ready\\n"); os._exit(0)'],
            stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=write_fd)
        os.close(write_fd)
        write_fd = -1
        if not select.select([process.stdout], [], [], 5)[0]:
            raise RuntimeError('command output was not forwarded')
        if process.stdout.readline() != b'ready\n':
            raise RuntimeError('command did not reach its successful exit')
        # Exceed the former 100 ms marker-only deadline without setting any
        # command deadline. The enclosing test harness still owns its timeout.
        time.sleep(0.25)
        captured = bytearray()
        end = time.monotonic() + 5
        while True:
            remaining = end - time.monotonic()
            if remaining <= 0 or not select.select(
                    [read_fd], [], [], remaining)[0]:
                raise RuntimeError('marker transfer did not finish after drain')
            chunk = os.read(read_fd, 65536)
            if not chunk:
                break
            captured.extend(chunk)
        status = process.wait(timeout=5)
        if status != 0:
            raise RuntimeError('successful command became exit ' + str(status))
        if captured[:filled] != b'x' * filled:
            raise RuntimeError('preexisting stderr bytes were changed')
        if not re.fullmatch(
                rb'\n# child-time: 1 [0-9]+\.[0-9]+ ms 0 0 '
                rb'marker-backpressure\n', captured[filled:]):
            raise RuntimeError('missing or corrupt final timing record')
    finally:
        if process is not None:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            process.stdout.close()
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)
    return None


try:
    executor = (sys.argv[1] if len(sys.argv) > 1 else
                os.environ.get('SIXEL_TEST_CHILD_TIME'))
    skip = check(str(Path(executor).resolve()) if executor else None)
except (OSError, RuntimeError, subprocess.SubprocessError) as error:
    print('1..1\nnot ok 1 - final child-time record survives backpressure')
    print('# ' + str(error), file=sys.stderr)
    sys.exit(1)
if skip:
    print('1..0 # SKIP ' + skip)
else:
    print('1..1\nok 1 - final child-time record survives backpressure')
