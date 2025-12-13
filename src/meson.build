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
srcs = [
  'output.c',
  'fromsixel.c',
  'tosixel.c',
  'timer.c',
  'assessment.c',
  'timer.h',
  'sleep.c',
  'sleep.h',
  'palette-heckbert.c',
  'palette-heckbert.h',
  'palette-common-merge.c',
  'palette-common-merge.h',
  'palette-common-snap.c',
  'palette-common-snap.h',
  'palette-kmeans.c',
  'palette-kmeans.h',
  'palette.c',
  'lookup-common.c',
  'lookup-8bit.c',
  'lookup-float32.c',
  'lookup-vpte-8bit.c',
  'lookup-vpte-float32.c',
  'dither.c',
  'dither-common-pipeline.c',
  'dither-positional-8bit.c',
  'dither-positional-float32.c',
  'dither-fixed-8bit.c',
  'dither-fixed-float32.c',
  'dither-varcoeff-8bit.c',
  'dither-varcoeff-float32.c',
  'logger.c',
  'logger.h',
  'colorspace.c',
  'frame.c',
  'cpu.c',
  'cpu.h',
  'pixelformat.c',
  'filter-colors.c',
  'filter-colors.h',
  'filter-factory.c',
  'filter-factory.h',
  'filter-final-merge.c',
  'filter-final-merge.h',
  'filter-dither.c',
  'filter-dither.h',
  'filter-lookup.c',
  'filter-lookup.h',
  'filter-clip.c',
  'filter-clip.h',
  'filter-resize.c',
  'filter-resize.h',
  'filter-sample.c',
  'filter-sample.h',
  'filter.c',
  'filter.h',
  'scale.c',
  'scale.h',
  'chunk.c',
  'loader.c',
  'loader-gdk-pixbuf2.c',
  'loader-gnome-thumbnailer.c',
  'loader-coregraphics.c',
  'loader-quicklook.c',
  'loader-wic.c',
  'loader-gd.c',
  'loader-libpng.c',
  'loader-libjpeg.c',
  'loader-builtin.c',
  'loader-common.c',
  'loader-registry.c',
  'frompnm.c',
  'fromgif.c',
  'clipboard.c',
  'encoder.c',
  'decoder.c',
  'decoder-parallel.c',
  'decoder-parallel.h',
  'decoder-image.h',
  'options.c',
  'writer.c',
  'stb_image_write.c',
  'status.c',
  'compat_stub.c',
  'malloc_stub.c',
  'allocator.c',
  'tty.c',
  'threading.c',
  'threading.h',
  'probe.c',
  'probe.h',
  'sixel_decode_rgba.c',
  'sixel_decode_rgba.h',
  'lookup-common.h',
  'lookup-8bit.h',
  'lookup-float32.h',
  'lookup-vpte-8bit.h',
  'lookup-vpte-float32.h',
  'dither-positional-8bit.h',
  'dither-positional-float32.h',
  'dither-fixed-8bit.h',
  'dither-fixed-float32.h',
  'dither-internal.h',
  'dither-varcoeff-8bit.h',
  'dither-varcoeff-float32.h',
  'options.h',
  'sixel_atomic.h',
]

if conf.get('SIXEL_ENABLE_THREADS', 0) == 1
  srcs += [
    'threadpool.c',
  ]
endif

if have_quicklook_thumbnailing
  srcs += ['quicklook_thumbnailing.m']
endif

if have_clipboard_macos
  srcs += ['clipboard_macos.m']
endif

lt_soversion = ls_lt_current - ls_lt_age
lt_version = '@0@.@1@.@2@'.format(lt_soversion, ls_lt_age, ls_lt_revision)

if get_option('amalgamation').enabled()
  libsixel_sources = [amalgamation_out]
else
  libsixel_sources = srcs
endif

libsixel = library('sixel',
                   libsixel_sources,
                   include_directories: project_inc,
                   c_args: libsixel_posix_defs,
                   dependencies: deps,
                   version: lt_version,
                   soversion: lt_soversion.to_string(),
                   install: true)

libsixel_dep = declare_dependency(link_with: libsixel,
                                  include_directories: project_inc,
                                  dependencies: deps)

if get_option('tests')
  executable('tests', ['tests.c'],
    dependencies: libsixel_dep,
    include_directories: project_inc,
    install: false)
endif

# vim: set et ts=2:
