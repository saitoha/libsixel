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

if get_option('img2sixel').enabled()
  executable('img2sixel', ['img2sixel.c'] + sources_common,
    dependencies: libsixel_dep,
    include_directories: project_inc,
    install: true)
  install_man('img2sixel.1')
  if bash_completion_dir != ''
    install_data('shell-completion/bash/img2sixel', install_dir: bash_completion_dir)
  endif
  if zsh_completion_dir != ''
    install_data('shell-completion/zsh/_img2sixel', install_dir: zsh_completion_dir)
  endif
endif

if get_option('sixel2png').enabled()
  executable('sixel2png', ['sixel2png.c'] + sources_common,
    dependencies: libsixel_dep,
    include_directories: project_inc,
    install: true)
  install_man('sixel2png.1')
  if bash_completion_dir != ''
    install_data('shell-completion/bash/sixel2png',
      install_dir: bash_completion_dir)
  endif
  if zsh_completion_dir != ''
    install_data('shell-completion/zsh/_sixel2png',
      install_dir: zsh_completion_dir)
  endif
endif

# vim: set et ts=2:
