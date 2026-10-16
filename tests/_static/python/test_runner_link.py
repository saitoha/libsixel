#!/usr/bin/env python3
"""Exercise the production make link rules without compiling test objects."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


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
            'pathlib.Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]))\n')
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

        # Updating the configured inventory must refresh the response file.
        makefile.write_text(makefile.read_text().replace(
            ' '.join(objects), ' '.join(objects[1:])))
        os.utime(inventory, (1, 1))
        os.utime(target, (1, 1))
        run()
        if inventory.read_text().splitlines() != objects[1:]:
            raise RuntimeError('the list stayed stale after a Makefile change')

        # A failed read must not invoke the linker with an empty object list.
        inventory.unlink()
        inventory.mkdir()
        target.unlink()
        result = subprocess.run(
            [make, '--no-print-directory', 'test_runner'], cwd=work,
            text=True, capture_output=True)
        if result.returncode == 0 or target.exists():
            raise RuntimeError('an unreadable inventory did not stop linking')


try:
    check(Path(sys.argv[1]).resolve())
except (OSError, RuntimeError) as error:
    print(error, file=sys.stderr)
    sys.exit(1)
