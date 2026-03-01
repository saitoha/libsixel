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
  '../src/allocator.c',
  '../src/status.c',
  '../src/timer.c',
  '../src/logger.c',
  '../src/probe.c',
  '../src/compat_stub.c',
  '../src/path.c',
)

if conf.get('SIXEL_ENABLE_THREADS', 0) == 1
  wic_sources += [
    '../src/threading.c',
    '../src/threadpool.c',
    '../src/decoder-parallel.c',
  ]
endif

wic_c_args = [
  '-DSIXEL_STATUS_HAVE_TTY_HELPERS=0',
  '-DHAVE_CONFIG_H',
]
wic_link_args = []

if (cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl') and msvc_pgo_mode != 'off'
  # This module is not exercised by the current PGO training commands.
  # Keep project-wide PGO for the core binaries, but opt out for WIC to
  # avoid C4961/C2220 when no wicsixel!*.pgc is produced.
  wic_c_args += ['/GL-']
  wic_link_args += ['/LTCG:OFF']
endif

shared_module(
  'wicsixel',
  wic_sources,
  include_directories: project_inc,
  dependencies: [ole32, oleaut32, gdi32, windowscodecs],
  c_args: wic_c_args,
  link_args: wic_link_args,
  install: true,
  install_dir: join_paths(get_option('libdir'), meson.project_name()),
)

if host_machine.system() == 'windows' and get_option('register_dll')
  dll_rel_win = join_paths( \
    get_option('libdir'), \
    meson.project_name(), \
    'libwicsixel.dll' \
  ).replace('/', '\\')

  meson.add_install_script(
    'powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
      'if (-not $env:MESON_INSTALL_DESTDIR) {'
    + '& "$env:SystemRoot\\System32\\regsvr32.exe" /s "$env:MESON_INSTALL_PREFIX\\' + dll_rel_win
    + '" }'
  )
endif

# vim: set et ts=2:
