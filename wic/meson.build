# SPDX-License-Identifier: MIT

wic_sources = files(
  'wic.c',
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

if host_machine.system() == 'windows'
  dll_rel_win = join_paths(get_option('libdir'), meson.project_name(), 'wicsixel.dll').replace('/', '\\')

  meson.add_install_script(
    'powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
      'if (-not $env:MESON_INSTALL_DESTDIR) {'
    + '& "$env:SystemRoot\\System32\\regsvr32.exe" /s "$env:MESON_INSTALL_PREFIX\\' + dll_rel_win
    + '" }'
  )
endif

# vim: set et ts=2:
