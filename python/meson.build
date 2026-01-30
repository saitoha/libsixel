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
python_opt = get_option('python')
python_wheel_opt = get_option('python_wheel')
if not python_opt.disabled()
  py_mod = import('python')
  py = py_mod.find_installation(
    required: python_opt.enabled() or python_wheel_opt.enabled()
  )
  if py.found()
    python_sources = files(
        'libsixel/__init__.py',
        'libsixel/_libs/__init__.py',
        'libsixel/encoder.py',
        'libsixel/decoder.py'
    )
    py.install_sources(python_sources, subdir: 'libsixel')
  endif
elif python_opt.enabled()
  error('Python support requires an interpreter')
endif

if python_wheel_opt.enabled()
  wheel_script = files('wheel_builder.py')
  wheel_distdir = join_paths(meson.current_build_dir(), 'dist')
  custom_target(
    'libsixel-python-wheel',
    input: libsixel_library,
    output: 'libsixel-wheel.stamp',
    command: [
      py, wheel_script,
      '--libpath', '@INPUT@',
      '--distdir', wheel_distdir
    ],
    build_by_default: true,
  )
endif

# vim: set et ts=2:
