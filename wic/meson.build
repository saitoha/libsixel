# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 libsixel developers. See `AUTHORS`.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

wic_sources = files(
  # Place wic_stub first so INITGUID definitions are emitted before wic.c
  # when unity builds concatenate sources. This prevents missing GUID
  # symbols such as GUID_MetadataFormatExif.
  'wic_stub.c',
  'wic.c',
  'wic_stub.h',
  '../src/fromsixel.c',
  '../src/colorspace-hls.c',
  '../src/cpu.c',
  '../src/allocator.c',
  '../src/status.c',
  '../src/timer.c',
  '../src/timeline-logger.c',
  '../src/timeline-writer.c',
  '../src/probe.c',
  '../src/compat_stub.c',
  '../src/path.c',
  '../src/threading.c',
  '../src/options-registry.c',
)

if conf.get('SIXEL_ENABLE_THREADS', 0) == 1
  wic_sources += [
    '../src/threadpool.c',
    '../src/decoder-parallel.c',
  ]
endif

wic_c_args = [
  '-DSIXEL_STATUS_HAVE_TTY_HELPERS=0',
  '-DSIXEL_DECODE_PIXELS_NO_FAST4=1',
  '-DHAVE_CONFIG_H',
]
# Static CRT/compiler runtime keeps Explorer independent of developer tools.
wic_link_args = []
if cc.get_argument_syntax() != 'msvc'
  wic_link_args += cc.get_supported_link_arguments('-static-libgcc', '-Wl,-Bstatic')
endif
wic_runtime_deps = []
if get_option('winpthread').enabled()
  wic_runtime_deps += cc.find_library('winpthread', static: true)
endif
wic_dll = shared_module(
  'wicsixel',
  wic_sources,
  name_prefix: 'lib',
  vs_module_defs: 'wicsixel.def',
  include_directories: project_inc,
  dependencies: [ole32, oleaut32, gdi32, uuid, windowscodecs, advapi32, user32] + wic_runtime_deps,
  c_args: wic_c_args,
  link_args: wic_link_args,
  override_options: ['b_vscrt=mt'],
  install: build_machine.system() != 'windows',
  install_dir: get_option('datadir') / 'libsixel/installers/wic-package',
)

if build_machine.system() == 'windows'
  wic_powershell = find_program('powershell.exe', native: true)
  wic_installer = custom_target(
    'wic-installer',
    input: wic_dll,
    output: 'libsixel-wic.msi',
    command: [wic_powershell, '-NoProfile', '-NonInteractive',
      '-ExecutionPolicy', 'Bypass', '-File', files('installer.ps1'),
      '-Dll', '@INPUT@', '-Version', meson.project_version(),
      '-License', meson.project_source_root() / 'LICENSE', '-Output', '@OUTPUT@'],
    depend_files: files('registry.tsv', '../LICENSE'),
    build_by_default: true,
    install: true,
    install_dir: get_option('datadir') / 'libsixel/installers',
  )
  meson.add_install_script(wic_powershell, '-NoProfile', '-NonInteractive',
    '-ExecutionPolicy', 'Bypass', '-File', files('installer.ps1'),
    '-Action', 'Install', '-Output', wic_installer.full_path())
  if get_option('tests')
    wic_installed_probe = executable('wic-installed-decode',
      '../tests/extension/wic-installed/decode-red-pixel.c',
      dependencies: [ole32, uuid, windowscodecs],
      build_by_default: false, install: false)
    wic_lifetime_probe = executable('wic-installed-lifetime',
      '../tests/extension/wic-installed/lifetime-probe.c',
      dependencies: [ole32, oleaut32, uuid, windowscodecs],
      build_by_default: false, install: false)
    wic_installcheck_command = [sh, '-c', '''
set -eu
log="$1"
shift
status=0
"$@" > "$log" 2>&1 || status=$?
cat "$log"
exit "$status"
''', 'wic-installcheck', meson.current_build_dir() / 'installcheck.log',
      sh, files('powershell.sh'), wic_powershell,
      files('../tests/extension/wic-installer/run-tests.ps1'),
      '-Mode', 'Installed', '-Dll', wic_dll.full_path(),
      '-Package', wic_installer.full_path(), '-Probe', wic_installed_probe.full_path(),
      '-LifetimeProbe', wic_lifetime_probe.full_path()]
    wic_installcheck_deps = [wic_installed_probe, wic_lifetime_probe, wic_installer]
    run_target('installcheck-wic', command: wic_installcheck_command,
      depends: wic_installcheck_deps)
    # Keep CI unaware of individual tests and preserve the install ordering.
    # The explicit lifecycle target installs the codec on the current machine.
    run_target('check-wic-installation',
      command: [sh, '-c', '''
set -eu
test -z "${DESTDIR-}${SIXEL_INSTALLCHECK_DESTDIR-}" || {
  echo "check-wic-installation requires a system install, not DESTDIR." >&2
  exit 1
}
"$1" test -C "$2" --no-rebuild --suite extension --print-errorlogs
"$1" install -C "$2" --no-rebuild
"$1" compile -C "$2" installcheck-wic
"$3" "$4" "$2/meson-logs/testlog.json" "$2/wic/installcheck.log"
''', 'wic-check-installation', find_program('meson'),
        meson.project_build_root(), python3,
        meson.project_source_root() / 'tools/check_test_times.py'],
      depends: wic_installcheck_deps)
    # Each numbered file emits its own TAP result in a shared PowerShell process.
    wic_test_executable = wic_powershell
    wic_test_args = ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
      '-File', files('../tests/extension/wic-installer/run-tests.ps1'),
      '-Dll', wic_dll, '-Package', wic_installer]
    wic_test_env = environment()
    if have_child_time_executor
      wic_test_env.set('LSO_CHILD_TIME', '1')
      wic_test_args = ['--record-child-time', 'wic-msi', '0', '--',
                       wic_powershell] + wic_test_args
      wic_test_executable = child_time_executor
    else
      wic_test_env.set('LSO_CHILD_TIME', child_time_fallback)
    endif
    test('wic-msi', wic_test_executable,
      args: wic_test_args, env: wic_test_env,
      suite: 'extension', protocol: 'tap')
  endif
else
  message('WIC cross build: DLL only; run wic/installer.ps1 on Windows to generate the MSI')
  wic_package_dir = get_option('datadir') / 'libsixel/installers/wic-package'
  install_data('installer.ps1', 'registry.tsv', '../LICENSE', install_dir: wic_package_dir)
endif

# vim: set et ts=2:
