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
have_python = false
have_wic = false

cc = meson.get_compiler('c')
cpu = host_machine.cpu_family()

have_unreachable = cc.compiles('''
  static void f(int x) {
    if (x) return;
    __builtin_unreachable();
  }
  int main(void){ f(0); return 0; }
''', name: '__builtin_unreachable()', args: ['-Werror'])
if have_unreachable
  conf.set('HAVE___BUILTIN_UNREACHABLE', 1)
endif

if cc.compiles('''
    __attribute__((deprecated)) void test(void) {}
''')
   has_func_attr_deprecated = true
   conf.set('HAVE_FUNC_ATTRIBUTE_DEPRECATED', 1)
endif

if cc.compiles('''
    __attribute__((deprecated)) int test;
''')
   has_var_attr_deprecated = true
   conf.set('HAVE_VAR_ATTRIBUTE_DEPRECATED', 1)
endif

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic pop
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    __attribute__((deprecated)) void test(void) {}
''')
  conf.set('HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS', 1)
endif

flags = cc.get_supported_arguments([
    '-Wformat=2',
    '-fno-emulated-tls',
    '-Bsymbolic',
])
add_project_arguments(flags, language: 'c')
foreach f: flags
  if f == '-fno-emulated-tls'
    conf.set('HAVE_FNO_EMULATED_TLS', 1)
  elif f == '-Bsymbolic'
    conf.set('HAVE_BSYMBOLIC', 1)
  endif
endforeach

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
]
foreach h : std_headers
  if cc.has_header(h)
    macro = 'HAVE_' + h.to_upper().replace('.', '_').replace('/', '_')
    conf.set(macro, 1)
  endif
endforeach

# miscellaneous functions
extra_funcs = [
  'malloc',
  'realloc',
  'calloc',
  'memcpy',
  'memmove',
  'memset',
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

if not cc.has_function('getopt_long') and cc.has_function('getopt')
    conf.set('HAVE_GETOPT', 1)
endif

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

deps = []
reqs = []
link_libs = []

m_dep = cc.find_library('m', required: false)
if m_dep.found()
  link_libs += m_dep
  deps += m_dep
  conf.set('HAVE_LIBM', 1)
endif

png_dep = dependency('libpng', required: get_option('png'))
if png_dep.found()
  deps += png_dep
  reqs += png_dep
  conf.set('HAVE_LIBPNG', 1)
  loaders += 'png'
  conf.set10('HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8',
             cc.has_function('png_set_gray_1_2_4_to_8',
                             dependencies: png_dep))
  conf.set10('HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8',
             cc.has_function('png_set_expand_gray_1_2_4_to_8',
                             dependencies: png_dep))
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
  conf.set('HAVE_LIBCURL', 1)
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
      loaders += 'wic'
      have_wic = true
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
      loaders += 'coregraphics'
    endif
  endif
elif cg_opt.enabled()
  error('CoreGraphics support requires macOS')
endif

simd_opt = get_option('simd')
if simd_opt.disabled()
  simd_message = 'no'
elif cpu == 'x86_64'
  if cc.has_header('emmintrin.h')
    conf.set('HAVE_EMMINTRIN_H', 1)
    conf.set('HAVE_SSE2', 1)
    simd_message = 'sse2'
  else
    simd_message = 'no'
  endif
elif cpu == 'x86'
  if cc.has_header('emmintrin.h')
    conf.set('HAVE_EMMINTRIN_H', 1)
    if cc.get_id() != 'msvc'
      if cc.has_argument('-msse2') and cc.compiles('''
          #include <emmintrin.h>
          int main(void){
            __m128i a = _mm_setzero_si128(); (void)a; return 0;
          }''', args: ['-msse2'])
        conf.set('HAVE_SSE2', 1)
        simd_message = 'sse2'
        add_project_arguments('-msse2', language: 'c')
      else
        simd_message = 'no'
      endif
    else
      if cc.has_argument('/arch:SSE2') and cc.compiles('''
          #include <emmintrin.h>
          int main(void){
            __m128i a = _mm_setzero_si128(); (void)a; return 0;
          }''', args: ['/arch:SSE2'])
        conf.set('HAVE_SSE2', 1)
        simd_message = 'sse2'
        add_project_arguments('/arch:SSE2', language: 'c')
      else
        simd_message = 'no'
      endif
    endif
  else
    simd_message = 'no'
  endif
elif cpu == 'aarch64'
  if cc.has_header('arm_neon.h')
    conf.set('HAVE_ARM_NEON_H', 1)
    conf.set('HAVE_NEON', 1)
    simd_message = 'neon'
  else
    simd_message = 'no'
  endif
elif cpu.startswith('arm') or cpu == 'aarch32'
  if cc.has_header('arm_neon.h')
    conf.set('HAVE_ARM_NEON_H', 1)
    if cc.get_id() == 'msvc'
      conf.set('HAVE_NEON', 1)
      simd_message = 'neon'
    else
      test_args = ['-mfpu=neon']
      if cc.has_multi_arguments(test_args) and cc.compiles('''
          #include <arm_neon.h>
          int main(void){
            int32x4_t v = vdupq_n_s32(0); (void)v; return 0;
          }''', args: test_args)
        conf.set('HAVE_NEON', 1)
        simd_message = 'neon'
        add_project_arguments(test_args, language: 'c')
      else
        simd_message = 'no 3'
      endif
    endif
  else
    simd_message = 'no 1'
  endif
else
  simd_message = 'no'
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
  else
    python_message = 'no'
  endif
elif py_opt.enabled()
  error('Python support requires an interpreter')
endif

subdir('include')
subdir('src')
subdir('converters')
if have_wic
  subdir('wic')
endif
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
pc_conf.set('LIBS_PRIVATE', '')  # TODO: how to get the correct linker flags for static linking in meson?

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
message('       SIMD                : ' + simd_message)
message('       gcov integration    : ' + (get_option('gcov') ? 'yes' : 'no'))
message('       debugging           : ' + (get_option('debug') ? 'yes' : 'no'))
message('       tests               : ' + (get_option('tests') ? 'yes' : 'no'))
message('')
# vim: set et ts=2:
