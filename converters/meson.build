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
sources_common = ['malloc_stub.c']

img2sixel_exe = []
if get_option('img2sixel').enabled()
  img2sixel_target = executable('img2sixel', ['img2sixel.c'] + sources_common,
    dependencies: libsixel_dep,
    include_directories: project_inc,
    install: true)
  img2sixel_exe += img2sixel_target
  install_man('img2sixel.1')
  if bash_completion_dir != ''
    install_data('shell-completion/bash/img2sixel', install_dir: bash_completion_dir)
  endif
  if zsh_completion_dir != ''
    install_data('shell-completion/zsh/_img2sixel', install_dir: zsh_completion_dir)
  endif
endif

sixel2png_exe = []
if get_option('sixel2png').enabled()
  sixel2png_target = executable('sixel2png', ['sixel2png.c'] + sources_common,
    dependencies: libsixel_dep,
    include_directories: project_inc,
    install: true)
  sixel2png_exe += sixel2png_target
  install_man('sixel2png.1')
endif

if get_option('tests')
  sh = find_program('sh')
  run_tests = meson.current_source_dir() / 'tests' / 'run-tests.sh'
  test_env = environment()

  config_data = configuration_data()
  config_data.set('abs_top_builddir', meson.project_build_root())
  config_data.set('abs_top_srcdir', meson.project_source_root())
  config_data.set('TESTS_HAVE_PNG', png_dep.found() ? '1' : '0')
  config_data.set('TESTS_HAVE_CURL', curl_dep.found() ? '1' : '0')
  config_data.set('TESTS_WANT_SIXEL2PNG',
    get_option('sixel2png').enabled() ? '1' : '0')
  config_data.set('TESTS_WINE', '')
  config_data.set('TESTS_WINEEXT', host_machine.system() == 'windows' ? '.exe' : '')

  config_sh = configure_file(
    input: 'tests/config.sh.in',
    output: 'config.sh',
    configuration: config_data,
  )

  test_env.set('TEST_CONFIG_SH', config_sh.full_path())

  test_deps = []
  test_deps += img2sixel_exe
  test_deps += sixel2png_exe

  test('converters-suite', sh,
    args: [run_tests],
    env: test_env,
    depends: test_deps)
endif

# vim: set et ts=2:
