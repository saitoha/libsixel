#!/usr/bin/env python3
"""Compile against the headers selected by the staged install's pkg-config."""
# Policy: docs/build/test-harness.md
# Policy: docs/misc/platforms/cosmopolitan.md
# Coverage: COSMO-05 COSMO-06
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import sys


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "_support"))
import native_process


def run(command, **kwargs):
    result = native_process.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{command!r}:\n{result.stdout}')
    return result.stdout


def check(args):
    if os.environ.get('CROSS_COMPILING') == 'yes':
        return 'the staged consumer probe requires a native build'
    pkgconfig = os.environ.get('SIXEL_TEST_PKG_CONFIG')
    # Configure uses false when no usable pkg-config was found.
    if not pkgconfig or pkgconfig == 'false':
        return 'pkg-config is unavailable'
    pkgconfig_command = [pkgconfig]
    if os.name == 'nt':
        # Configure probes this command through its POSIX shell. A native
        # Python PATH lookup can select a different .BAT launcher instead.
        # Preserve that shell boundary for executable scripts and their args.
        shell = os.environ.get('SIXEL_TEST_CONFIG_SHELL')
        if not shell:
            raise RuntimeError('the configured test shell is unavailable')
        pkgconfig_command = [shell, '-c', 'exec "$@"', 'pkg-config', pkgconfig]
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
        flags = shlex.split(run(pkgconfig_command + ['--cflags', 'libsixel'],
                                env=env))
        headers = list(stage.rglob('sixel.h'))
        if len(headers) != 1:
            raise RuntimeError('expected exactly one staged sixel.h')
        directory = headers[0].parent
        # Native Windows and its POSIX tools can spell the same directory
        # with different separators and drive-letter case.
        if not any(flag.startswith('-I') and Path(flag[2:]) == directory
                   for flag in flags):
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
        selected_headers = set()
        for line in preprocessed.splitlines():
            if line.startswith('# '):
                fields = shlex.split(line)
                if len(fields) >= 3 and fields[1].isdigit():
                    selected_headers.add(Path(fields[2]))
        for name in ('sixel.h', '6cells.h'):
            if directory / name not in selected_headers:
                raise RuntimeError(f'consumer did not select staged {name}')
    return None


parser = argparse.ArgumentParser(description=__doc__)
# Autotools supplies the configured build through the common TAP environment;
# Meson retains explicit arguments for its separately registered native test.
parser.add_argument('backend', choices=('autotools', 'meson'), nargs='?',
                    default='autotools')
parser.add_argument('build', nargs='?', default=os.environ.get('TOP_BUILDDIR'))
parser.add_argument('installer', nargs='?',
                    default=os.environ.get('SIXEL_TEST_MAKE'))
parser.add_argument('source', nargs='?', default=os.environ.get('TOP_SRCDIR'))
parser.add_argument('--cc', default=os.environ.get('SIXEL_TEST_CC'))
args = parser.parse_args()
if not all((args.build, args.installer, args.source, args.cc)):
    parser.error('build, installer, source and compiler must be configured')
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
