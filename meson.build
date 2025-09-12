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
project('libsixel',
        'c',
        version: '1.8.7',
        license: 'MIT',
        default_options: [
            'buildtype=release',
            'c_std=c99',
            'warning_level=3'
        ])

host_system = host_machine.system()
add_project_arguments(
    '-D_POSIX_SOURCE -D_POSIX_C_SOURCE=199309L',
    language: 'c')

# configuration data
conf = configuration_data()
ls_lt_current = 1
ls_lt_revision = 6
ls_lt_age = 0
ls_lt_version = '@0@:@1@:@2@'.format(ls_lt_current, ls_lt_revision, ls_lt_age)

conf.set('LS_LTVERSION', ls_lt_version)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING',
                meson.project_name() + ' ' + meson.project_version())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_BUGREPORT', 'saitoha@me.com')
conf.set_quoted('PACKAGE_URL', '')
conf.set_quoted('VERSION', meson.project_version())

if get_option('gcov')
  add_project_arguments('--coverage', language: 'c')
  add_project_link_arguments('--coverage', language: 'c')
endif
if get_option('debug')
  conf.set('HAVE_DEBUG', 1)
endif
if get_option('tests')
  conf.set('HAVE_TESTS', 1)
endif

# bookkeeping for configuration summary
loaders = ['stb_image']
net = []
have_winpthread = false
python_message = 'no'
have_python = false

cc = meson.get_compiler('c')
cc.has_function('unreachable')
if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic pop
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    __attribute__((deprecated)) void test(void) {}
''')
  conf.set('HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS', 1)
endif

add_project_arguments(
  cc.get_supported_arguments([
    '-Wformat=2',
    '-fno-emulated-tls',
    '-Bsymbolic',
  ]), language: 'c')

# check common headers
std_headers = [
    'assert.h',
    'math.h',
    'ctype.h',
    'memory.h',
    'unistd.h',
    'stdint.h',
    'sys/unistd.h',
    'getopt.h',
    'sys/types.h',
    'sys/stat.h',
    'fcntl.h',
    'io.h',
    'errno.h',
    'limits.h',
    'sys/time.h',
    'time.h',
    'signal.h',
    'sys/select.h',
    'sys/signal.h',
    'termios.h',
    'sys/ioctl.h',
    'inttypes.h',
    'stdio.h',
    'stdlib.h',
    'string.h',
    'stddef.h'
]
foreach h : std_headers
  if cc.has_header(h)
    macro = 'HAVE_' + h.to_upper().replace('.', '_').replace('/', '_')
    conf.set(macro, 1)
  endif
endforeach

# memory functions and headers
if cc.has_function('memcpy')
  conf.set('HAVE_MEMCPY', 1)
endif
if cc.has_function('memmove')
  conf.set('HAVE_MEMMOVE', 1)
endif
if cc.has_function('memset')
  conf.set('HAVE_MEMSET', 1)
endif

# miscellaneous functions
extra_funcs = [
  'getopt',
  'getopt_long',
  'memcpy',
  'memmove',
  'setmode',
  '_setmode',
  'signal',
  'strdup',
  'strtoul',
  'calloc',
  'clearerr',
  'stat',
  'setjmp',
  'longjmp',
  'strerror',
  'isatty',
  'strncmp',
  'ldiv',
  'floor',
  'localeconv',
  'pow',
  'select',
  'sqrt',
  'strchr',
  'strerror',
  'strstr',
  'strtol'
]
foreach f : extra_funcs
  if cc.has_function(f)
    conf.set('HAVE_' + f.to_upper().replace('.', '_'), 1)
  endif
endforeach

if host_machine.system() != 'windows'
  if cc.has_function('clock')
    conf.set('HAVE_CLOCK', 1)
  endif
  if cc.has_function('nanosleep')
    conf.set('HAVE_NANOSLEEP', 1)
  endif
endif

# signal handling
foreach s : ['SIGINT', 'SIGTERM', 'SIGHUP']
  if cc.has_header_symbol('signal.h', s)
    conf.set('HAVE_DECL_' + s, 1)
  endif
endforeach

# detect attributes
if cc.has_function_attribute('deprecated')
  conf.set('attr_func_deprecated', '__attribute__((deprecated))')
  conf.set('attr_var_deprecated', '__attribute__((deprecated))')
endif

deps = []
reqs = []
link_libs = []
libpriv_flags = []

m_dep = cc.find_library('m', required: false)
if m_dep.found()
  link_libs += m_dep
  deps += m_dep
  libpriv_flags += '-lm'
endif

png_dep = dependency('libpng', required: get_option('png'))
if png_dep.found()
  deps += png_dep
  reqs += png_dep
  conf.set('HAVE_PNG', 1)
  loaders += 'png'
endif

jpeg_dep = dependency('libjpeg', required: get_option('jpeg'))
if jpeg_dep.found()
  deps += jpeg_dep
  reqs += jpeg_dep
  conf.set('HAVE_JPEG', 1)
  loaders += 'jpeg'
endif

curl_dep = dependency('libcurl', required: get_option('curl'))
if curl_dep.found()
  deps += curl_dep
  reqs += curl_dep
  conf.set('HAVE_CURL', 1)
  net += 'libcurl'
endif

gdk_dep = dependency('gdk-pixbuf-2.0', required: get_option('gdk_pixbuf2'))
if gdk_dep.found()
  deps += gdk_dep
  reqs += gdk_dep
  conf.set('HAVE_GDK_PIXBUF2', 1)
  loaders += 'gdk-pixbuf2'
endif

gd_dep = dependency('gd', required: get_option('gd'))
if not gd_dep.found()
  gd_dep = dependency('gdlib', required: get_option('gd'))
endif
if gd_dep.found()
  deps += gd_dep
  reqs += gd_dep
  conf.set('HAVE_GD', 1)
  loaders += 'gd'
endif

wic_opt = get_option('wic')
if host_system == 'windows'
  if not wic_opt.disabled()
    have_wic_headers = cc.has_header('windows.h') and cc.has_header('wincodec.h')
    ole32 = cc.find_library('ole32', required: false)
    windowscodecs = cc.find_library('windowscodecs', required: false)
    if have_wic_headers and ole32.found() and windowscodecs.found()
      conf.set('HAVE_WIC', 1)
      deps += [ole32, windowscodecs]
      link_libs += [ole32, windowscodecs]
      libpriv_flags += ['-lole32', '-lwindowscodecs']
      loaders += 'wic'
    elif wic_opt.enabled()
      error('WIC support requested, but required headers or libraries were not found')
    endif
  endif
elif wic_opt.enabled()
  error('WIC support is only available on Windows')
endif

winhttp_opt = get_option('winhttp')
if host_system == 'windows'
  if not winhttp_opt.disabled()
    have_winhttp_headers = cc.has_header('windows.h') and cc.has_header('winhttp.h')
    winhttp_lib = cc.find_library('winhttp', required: false)
    if have_winhttp_headers and winhttp_lib.found()
      conf.set('HAVE_WINHTTP', 1)
      deps += winhttp_lib
      link_libs += winhttp_lib
      libpriv_flags += '-lwinhttp'
      net += 'winhttp'
    elif winhttp_opt.enabled()
      error('WinHTTP support requested, but required headers or library were not found')
    endif
  endif
elif winhttp_opt.enabled()
  error('WinHTTP support is only available on Windows')
endif

winpthread_opt = get_option('winpthread')
if host_system == 'windows'
  if winpthread_opt.enabled()
    winpthread_lib = cc.find_library('winpthread', required: true)
    conf.set('WITH_WINPTHREAD', 1)
    conf.set('HAVE_NANOSLEEP', 1)
    conf.set('HAVE_CLOCK', 1)
    deps += winpthread_lib
    libpriv_flags += '-lwinpthread'
    link_libs += winpthread_lib
  endif
elif winpthread_opt.enabled()
  error('winpthread is only available on Windows')
endif

cg_opt = get_option('coregraphics')
if host_system == 'darwin'
  if not cg_opt.disabled()
    cg_modules = ['CoreGraphics', 'CoreFoundation', 'ImageIO']
    cg_dep = dependency('appleframeworks', modules: cg_modules, required: cg_opt.enabled())
    if cg_dep.found()
      conf.set('HAVE_COREGRAPHICS', 1)
      deps += cg_dep
      link_libs += cg_dep
      libpriv_flags += ['-framework CoreGraphics', '-framework CoreFoundation', '-framework ImageIO']
      loaders += 'coregraphics'
    endif
  endif
elif cg_opt.enabled()
  error('CoreGraphics support requires macOS')
endif

config_h = configure_file(output: 'config.h', configuration: conf)
project_inc = include_directories('.', 'include', 'src')

bash_completion_dir = get_option('bashcompletiondir')
if bash_completion_dir == ''
  bash_completion_dir = join_paths(get_option('datadir'), 'bash-completion', 'completions')
endif

zsh_completion_dir = get_option('zshcompletiondir')
if zsh_completion_dir == ''
  zsh_completion_dir = join_paths(get_option('datadir'), 'zsh', 'site-functions')
endif

py_opt = get_option('python')
if not py_opt.disabled()
  py_mod = import('python')
  py = py_mod.find_installation(required: py_opt.enabled())
  if py.found()
    have_python = true
    python_message = 'yes: ' + py.get_install_dir()
  elif py_opt.enabled()
    error('Python support requires an interpreter')
  endif
elif py_opt.enabled()
  error('Python support requires an interpreter')
endif

subdir('include')
subdir('src')
subdir('converters')
if have_python
  subdir('python')
endif

pkgconfig_dir_rel = join_paths(get_option('libdir'), 'pkgconfig')
pkgconfig_dir = join_paths(get_option('prefix'), pkgconfig_dir_rel)

req_names = []
foreach r : reqs
  req_names += r.name()
endforeach

pc_conf = configuration_data()
pc_conf.set('prefix', get_option('prefix'))
pc_conf.set('exec_prefix', '${prefix}')
pc_conf.set('libdir', join_paths('${exec_prefix}', get_option('libdir')))
pc_conf.set('includedir', join_paths('${prefix}', get_option('includedir')))
pc_conf.set('PACKAGE_DESCRIPTION', 'A lightweight, fast implementation of DEC SIXEL graphics codec')
pc_conf.set('PACKAGE_VERSION', meson.project_version())
pc_conf.set('REQUIRES_PRIVATE', ' '.join(req_names))
pc_conf.set('LIBS_PRIVATE', ' '.join(libpriv_flags))

configure_file(
  input: 'libsixel.pc.in',
  output: 'libsixel.pc',
  configuration: pc_conf,
  install: true,
  install_dir: pkgconfig_dir_rel)

net_summary = ' '.join(net)
loaders_summary = ' '.join(loaders)

message('')
message('libsixel was configured as follows')
message('')
message('       Loader component    : ' + loaders_summary)
message('       Network access      : ' + net_summary)
message('       libwinpthread       : ' + (have_winpthread ? 'yes' : 'no'))
message('       pkg-config dir      : ' + pkgconfig_dir)
message('       Bash completion dir : ' + bash_completion_dir)
message('       Zsh completion dir  : ' + zsh_completion_dir)
message('       python bindings     : ' + python_message)
message('       gcov integration    : ' + (get_option('gcov') ? 'yes' : 'no'))
message('       debugging           : ' + (get_option('debug') ? 'yes' : 'no'))
message('       tests               : ' + (get_option('tests') ? 'yes' : 'no'))
message('')
# vim: set et ts=2:
