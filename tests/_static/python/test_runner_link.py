#!/usr/bin/env python3
"""Exercise the production make link rules without compiling test objects."""

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def check_openvms_archives(root):
    """Check the config.status repair with a lost Automake dependency list."""
    make = shutil.which('gmake') or shutil.which('make')
    configure = (root / 'configure.ac').read_text()
    repair = configure.split("<<'SIXEL_OPENVMS_TEST_OBJECTS'\n", 1)[1]
    repair = repair.split('\nSIXEL_OPENVMS_TEST_OBJECTS', 1)[0]
    source = (root / 'tests/Makefile.am').read_text()
    archives = re.search(r'^check_LTLIBRARIES = (.+)$', source, re.M)
    if not archives:
        raise RuntimeError('the ordinary runner has no check-only archives')
    with tempfile.TemporaryDirectory(prefix='libsixel-vms-link-') as tmp:
        work = Path(tmp)
        (work / 'runner.o').touch()
        # GNV can lose the generated dependency list even though LDADD
        # retains the archive. A later check-am goal cannot repair ordering.
        # Use the actual config.status fragment, without compiling C.
        (work / 'Makefile').write_text(
            'check_LTLIBRARIES = ' + archives[1] + '\n'
            'am_test_runner_OBJECTS = runner.o\n'
            'test_runner_DEPENDENCIES =\n'
            'test_runner:\n'
            '\t@test "$(FAIL_LINK)" != yes\n'
            '\t@for archive in $(check_LTLIBRARIES); do '
            'test -f $$archive || exit 1; done\n'
            '\t@touch $@\n'
            '$(check_LTLIBRARIES):\n'
            '\t@test "$(FAIL_ARCHIVE)" != yes\n'
            '\t@touch $@\n' + repair + '\n')

        def run(*args, success=True):
            result = subprocess.run(
                [make, '--no-print-directory', *args, 'test_runner',
                 *archives[1].split()], cwd=work, text=True,
                capture_output=True,
                env={k: v for k, v in os.environ.items()
                     if k not in ('MAKEFLAGS', 'MFLAGS', 'MAKELEVEL')})
            if (result.returncode == 0) != success:
                raise RuntimeError(result.stdout + result.stderr)

        run('-n')
        if any((work / name).exists() for name in archives[1].split()):
            raise RuntimeError('OpenVMS dry-run built a test archive')
        run('FAIL_ARCHIVE=yes', success=False)
        if (work / 'test_runner').exists():
            raise RuntimeError('failed archive creation still linked the runner')
        run()
        run('FAIL_LINK=yes')
        # Library changes must invalidate the runner independently of objects.
        os.utime(work / 'runner.o', (1, 1))
        os.utime(work / 'test_runner', (1, 1))
        run('FAIL_LINK=yes', success=False)


def check(root):
    make = shutil.which('gmake') or shutil.which('make')
    if not make:
        raise RuntimeError('GNU make is required for the link-rule check')
    rules = root / 'build-aux/test-runner-link.am'
    with tempfile.TemporaryDirectory(prefix='libsixel-link-') as tmp:
        work = Path(tmp)
        # Repeat two files to exceed Linux's single-argument limit with
        # little fixture I/O. Order and duplicates must survive unchanged.
        names = ['test_runner-' + letter * 160 + '.o' for letter in 'ab']
        objects = names * 1025
        for name in names:
            (work / name).touch()
        (work / 'verify.py').write_text(
            'import json, pathlib, sys\n'
            'args = []\n'
            'for arg in sys.argv[2:]:\n'
            '    args.extend(pathlib.Path(arg[1:]).read_text().splitlines()\n'
            '                if arg.startswith("@") else [arg])\n'
            'pathlib.Path(sys.argv[1]).write_text(json.dumps(args))\n')
        makefile = work / 'Makefile'
        makefile.write_text(
            'test_runner_OBJECTS = ' + ' '.join(objects) + '\n'
            'test_runner_LINK = "' + sys.executable + '" verify.py $@ before\n'
            'test_runner_LDADD = library-after-objects\n'
            'LIBS = last-library\n'
            'include ' + str(rules) + '\n')

        def run(*args):
            result = subprocess.run(
                [make, '--no-print-directory', *args, 'test_runner'],
                cwd=work, text=True, capture_output=True,
                env={k: v for k, v in os.environ.items()
                     if k not in ('MAKEFLAGS', 'MFLAGS', 'MAKELEVEL')})
            if result.returncode:
                raise RuntimeError(result.stdout + result.stderr)
            return result.stdout

        dry = run('-n')
        if (work / 'test_runner.objects').exists():
            raise RuntimeError('make -n wrote the object inventory')
        if max(map(len, dry.splitlines())) >= 32768:
            raise RuntimeError('a generated recipe exceeds 32 KiB')
        run('-j2')
        inventory = work / 'test_runner.objects'
        if inventory.read_text().splitlines() != objects:
            raise RuntimeError('the object inventory changed order or count')
        expected = ['before', *objects, 'library-after-objects', 'last-library']
        target = work / 'test_runner'
        if json.loads(target.read_text()) != expected:
            raise RuntimeError('link argv lost objects or changed library order')
        stamps = inventory.stat().st_mtime_ns, target.stat().st_mtime_ns
        run()
        if stamps != (inventory.stat().st_mtime_ns, target.stat().st_mtime_ns):
            raise RuntimeError('an up-to-date runner rewrote or relinked files')

        # The configured fast path must give the compiler one response-file
        # argument before libraries, while preserving the exact inventory.
        target.unlink()
        dry = run('-n', 'SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes')
        if '@test_runner.objects' not in dry or 'objects=$(cat' in dry:
            raise RuntimeError('the fast path expanded objects in the shell')
        run('SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes')
        if json.loads(target.read_text()) != expected:
            raise RuntimeError('response-file linking changed argument order')
        stamps = inventory.stat().st_mtime_ns, target.stat().st_mtime_ns
        run('SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=yes')
        if stamps != (inventory.stat().st_mtime_ns, target.stat().st_mtime_ns):
            raise RuntimeError('response-file linking was not incremental')

        # Updating the configured inventory must refresh the response file.
        makefile.write_text(makefile.read_text().replace(
            ' '.join(objects), ' '.join(objects[1:])))
        os.utime(inventory, (1, 1))
        os.utime(target, (1, 1))
        run()
        if inventory.read_text().splitlines() != objects[1:]:
            raise RuntimeError('the list stayed stale after a Makefile change')

        # Neither path may publish a target from an unreadable inventory.
        inventory.unlink()
        inventory.mkdir()
        target.unlink()
        for mode in ('no', 'yes'):
            result = subprocess.run(
                [make, '--no-print-directory',
                 'SIXEL_TEST_RUNNER_USE_RESPONSE_FILE=' + mode, 'test_runner'],
                cwd=work, text=True, capture_output=True)
            if result.returncode == 0 or target.exists():
                raise RuntimeError('an unreadable inventory did not stop linking')



try:
    check(Path(sys.argv[1]).resolve())
    check_openvms_archives(Path(sys.argv[1]).resolve())
except (OSError, RuntimeError) as error:
    print(error, file=sys.stderr)
    sys.exit(1)
