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

sources_common = [
    'malloc_stub.c',
    'compat_stub.c'
]

# +--------------------------------------------------------------+
# | Embedded completion header generation pipeline (ASCII view). |
# +--------------------------------------------------------------+
# bash/img2sixel ----\
#                      >--[gen_completion_embed.py]--> completion_embed.h
# zsh/_img2sixel  ----/
gen_completion = find_program(
  join_paths(meson.project_source_root(), 'tools', 'gen_completion_embed.py'),
  required: true,
)
completion_embed = custom_target('completion_embed.h',
  input: [
    'shell-completion/bash/img2sixel',
    'shell-completion/zsh/_img2sixel',
  ],
  output: 'completion_embed.h',
  command: [gen_completion, '@OUTPUT@', '@INPUT0@', '@INPUT1@'],
)
local_inc = include_directories('.')

img2sixel_opt = get_option('img2sixel')
sixel2png_opt = get_option('sixel2png')

img2sixel_deps = []
if img2sixel_opt.enabled()
  img2sixel_exe = executable('img2sixel',
    ['img2sixel.c', 'completion_utils.c', completion_embed] + sources_common,
    dependencies: libsixel_dep,
    include_directories: [project_inc, local_inc],
    install: true)
  img2sixel_deps = [img2sixel_exe]
  install_man('img2sixel.1')
  if bash_completion_dir != ''
    install_data('shell-completion/bash/img2sixel', install_dir: bash_completion_dir)
  endif
  if zsh_completion_dir != ''
    install_data('shell-completion/zsh/_img2sixel', install_dir: zsh_completion_dir)
  endif
endif

sixel2png_deps = []
if sixel2png_opt.enabled()
  sixel2png_exe = executable('sixel2png', ['sixel2png.c'] + sources_common,
    dependencies: libsixel_dep,
    include_directories: project_inc,
    install: true)
  install_man('sixel2png.1')
  sixel2png_deps = [sixel2png_exe]
endif

if img2sixel_opt.enabled()
  bash = find_program('bash', required: false)
  if not bash.found()
    message('bash not found; converter tests skipped')
  else
    env = environment()
    env.set('TOP_SRCDIR', meson.source_root())
    env.set('SRC_DIR', meson.current_source_dir())
    env.set('BUILD_DIR', meson.current_build_dir())
    env.set('TMP_DIR', join_paths(meson.current_build_dir(), 'tmp'))
    wine = run_command('sh', '-c', 'printf %s "${WINE:-}"', check: false).stdout().strip()
    if wine != ''
      env.set('WINE', wine)
    endif
    wineext = run_command('sh', '-c', 'printf %s "${WINEEXT:-}"', check: false).stdout().strip()
    if wineext != ''
      env.set('WINEEXT', wineext)
    endif

    test_scripts = [
      'test_0001_invalid_option.bash',
      'test_0002_stdin.bash',
      'test_0003_information.bash',
      'test_0004_conversion_options.bash',
      'test_0005_dcs_arguments.bash',
      'test_0006_dcs_variations.bash',
      'test_0007_animation.bash',
      'test_0008_progressive_jpeg.bash',
    ]
    if png_dep.found()
      test_scripts += ['test_0009_pngsuite.bash']
    endif
    if curl_dep.found()
      test_scripts += ['test_0010_curl.bash']
    endif
    if sixel2png_opt.enabled()
      test_scripts += ['test_0011_basic.bash']
    endif
    test_scripts += [
      'test_0012_regressions.bash',
      'test_0013_documentation.bash',
    ]

    tests_dir = join_paths(meson.source_root(), 'tests', 'converters')
    order = 0
    foreach script : test_scripts
      deps = []
      deps += img2sixel_deps
      if script == 'test_0011_basic.bash'
        deps += sixel2png_deps
      endif
      test(script, bash,
        args: ['-xv', join_paths(tests_dir, script)],
        env: env,
        depends: deps,
        is_parallel: false,
        priority: -order)
      order += 1
    endforeach
  endif
endif

# vim: set et ts=2:
