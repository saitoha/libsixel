#!/usr/bin/env python3
"""Compile against the headers selected by the staged install's pkg-config."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def run(command, **kwargs):
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{command!r}:\n{result.stdout}')
    return result.stdout


def check(args):
    pkgconfig = shutil.which('pkg-config')
    if not pkgconfig:
        return 'pkg-config is unavailable'
    compiler = shlex.split(args.cc)
    if any(Path(part).name.lower() in ('cl', 'cl.exe', 'clang-cl',
                                      'clang-cl.exe') for part in compiler):
        return 'the staged consumer probe requires Unix compiler flags'
    build = Path(args.build).resolve()
    with tempfile.TemporaryDirectory(prefix='sixel-install-') as temporary:
        stage = Path(temporary)
        if args.backend == 'meson':
            run([args.installer, 'install', '-C', str(build), '--no-rebuild',
                 '--tags', 'devel', '--destdir', str(stage)])
        else:
            # Only the public development files are needed; avoid installing
            # optional plugins or running their platform registration hooks.
            run([args.installer, '-C', str(build / 'include'), 'install',
                 'DESTDIR=' + str(stage)])
            run([args.installer, '-C', str(build), 'install-pkgconfigDATA',
                 'DESTDIR=' + str(stage)])
        packages = list(stage.rglob('libsixel.pc'))
        if len(packages) != 1:
            raise RuntimeError('expected exactly one staged libsixel.pc')
        # Keep /usr/include visible in Cflags for a staged system-prefix
        # install; host compiler defaults must not satisfy this probe.
        env = dict(os.environ, PKG_CONFIG_PATH=str(packages[0].parent),
                   PKG_CONFIG_SYSROOT_DIR=str(stage),
                   PKG_CONFIG_ALLOW_SYSTEM_CFLAGS='1')
        flags = shlex.split(run([pkgconfig, '--cflags', 'libsixel'], env=env))
        headers = list(stage.rglob('sixel.h'))
        if len(headers) != 1:
            raise RuntimeError('expected exactly one staged sixel.h')
        directory = headers[0].parent
        if '-I' + str(directory) not in flags:
            raise RuntimeError(f'Cflags {flags!r} do not select {directory}')
        for name in ('sixel.h', '6cells.h'):
            expected = build / 'include' / name
            if not expected.exists():
                expected = Path(args.source) / 'include' / name
            if (directory / name).read_bytes() != expected.read_bytes():
                raise RuntimeError(f'installed {name} differs from this build')
        source = stage / 'consumer.c'
        source.write_text('#include <sixel.h>\n#include <6cells.h>\n'
                          'int main(void) { return 0; }\n')
        run(compiler + flags + ['-c', str(source), '-o',
                               str(stage / 'consumer.o')])
        # Line markers prove actual header selection, even if another install
        # is visible through compiler defaults or environment include paths.
        preprocessed = run(compiler + flags + ['-E', str(source)])
        for name in ('sixel.h', '6cells.h'):
            if '"' + str(directory / name) + '"' not in preprocessed:
                raise RuntimeError(f'consumer did not select staged {name}')
    return None


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('backend', choices=('autotools', 'meson'))
parser.add_argument('build')
parser.add_argument('installer')
parser.add_argument('source')
parser.add_argument('--cc', required=True)
args = parser.parse_args()
try:
    skip = check(args)
except (OSError, RuntimeError, subprocess.SubprocessError) as error:
    print('1..1\nnot ok 1 - pkg-config selects the staged public headers')
    print('# ' + str(error).replace('\n', '\n# '))
    raise SystemExit(1)
if skip:
    print('1..0 # SKIP ' + skip)
else:
    print('1..1\nok 1 - pkg-config selects the staged public headers')
