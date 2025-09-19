# SPDX-License-Identifier: MIT

wic_sources = files(
  'wic.c',
  'wic_stub.c',
  '../src/fromsixel.c',
  '../src/allocator.c',
  '../src/status.c',
)

shared_module(
  'wicsixel',
  wic_sources,
  include_directories: project_inc,
  dependencies: [ole32, windowscodecs],
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
