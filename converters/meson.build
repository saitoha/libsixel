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

sources_common = ['malloc_stub.c', 'cli.c']
aborttrace_sources = ['aborttrace.c']
aborttrace_deps_local = []
if abort_trace_enabled
  aborttrace_deps_local = abort_trace_deps
endif

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

# for build_rpath
src_build_dir = join_paths(meson.project_build_root(), 'src')

img2sixel_opt = get_option('img2sixel')
sixel2png_opt = get_option('sixel2png')
amalgamated_tools_enabled = get_option('amalgamated_tools').enabled()

img2sixel_deps = []
if img2sixel_opt.enabled()
  if amalgamated_tools_enabled
    img2sixel_exe = executable('img2sixel', amalgamation_out,
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H'],
      include_directories: [project_inc, local_inc],
      dependencies: deps + aborttrace_deps_local,
      install: true)
  else
    img2sixel_exe = executable('img2sixel',
      ['img2sixel.c', 'completion_utils.c', completion_embed]
        + sources_common + aborttrace_sources,
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: [project_inc, local_inc],
      build_rpath: src_build_dir,
      install: true)
  endif
  img2sixel_deps = [img2sixel_exe]
  install_man('img2sixel.1')
  if bash_completion_dir != ''
    # Explicitly tag the completion install so Meson does not try to
    # guess an absolute path outside the prefix on Windows runners.
    install_data('shell-completion/bash/img2sixel',
                 install_dir: bash_completion_dir,
                 install_tag: 'data')
  endif
  if zsh_completion_dir != ''
    # See the note above for the bash completion install.
    install_data('shell-completion/zsh/_img2sixel',
                 install_dir: zsh_completion_dir,
                 install_tag: 'data')
  endif
endif

sixel2png_deps = []
if sixel2png_opt.enabled()
  if amalgamated_tools_enabled
    sixel2png_exe = executable('sixel2png', amalgamation_out,
      c_args: libsixel_posix_defs + ['-DBUILD_SIXEL2PNG'],
      include_directories: [project_inc, local_inc],
      dependencies: deps + aborttrace_deps_local,
      install: true)
  else
    sixel2png_exe = executable('sixel2png',
      ['sixel2png.c'] + sources_common + aborttrace_sources,
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: project_inc,
      build_rpath: src_build_dir,
      install: true)
  endif
  install_man('sixel2png.1')
  sixel2png_deps = [sixel2png_exe]
endif

# vim: set et ts=2:
