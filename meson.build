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
        version: '1.11.0-pre',
        meson_version: '>=0.59.0',
        license: 'MIT',
        default_options: [
            'buildtype=plain',
            'c_std=c99',
            'warning_level=3'
        ])

host_system = host_machine.system()
cc = meson.get_compiler('c')
fs = import('fs')
conf = configuration_data()
have_objc = add_languages('objc', required: false, native: false)
if have_objc
  objc = meson.get_compiler('objc')
  message('Objective-C compiler found: ' + objc.get_id())
else
  message('Objective-C compiler not available, skipping ObjC parts')
endif

# Keep Meson LTO behavior explicit on Windows toolchains.
#
# - MSVC (cl.exe) supports full LTO-style code generation via /GL+/LTCG,
#   but does not support LLVM ThinLTO mode.
# - clang-cl supports ThinLTO only when used with lld-link.
if get_option('b_lto')
  lto_mode = get_option('b_lto_mode')
  if cc.get_id() == 'msvc' and lto_mode == 'thin'
    error('b_lto_mode=thin is unsupported with MSVC (cl.exe). Use b_lto_mode=default.')
  endif

  if cc.get_id() == 'clang-cl' and lto_mode == 'thin'
    if cc.get_linker_id() != 'lld-link'
      error('b_lto_mode=thin with clang-cl requires lld-link.')
    endif
  endif
endif

if host_system == 'darwin'
  add_project_arguments('-D_DARWIN_C_SOURCE', language: 'c')
  conf.set('_DARWIN_C_SOURCE', 1)
endif

# Always expose generated config.h so headers stay reachable across platforms.
add_project_arguments('-DHAVE_CONFIG_H', language: 'c')
add_project_arguments('-DHAVE_CONFIG_H', language: 'objc')

# configuration data
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

msvc_pgo_mode = get_option('msvc_pgo')
msvc_pgo_data = get_option('msvc_pgo_data')
msvc_pgo_summary = 'off'
if cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
  if msvc_pgo_mode != 'off'
    # Keep PGO flags explicit for MSVC-like frontends because Meson base
    # option b_pgo currently targets GNU/LLVM-style -fprofile-* flags.
    add_project_arguments('/GL', language: 'c')

    msvc_pgo_link_args = []
    if msvc_pgo_mode == 'generate'
      msvc_pgo_link_args += ['/LTCG:PGINSTRUMENT']
    elif msvc_pgo_mode == 'use'
      msvc_pgo_link_args += ['/LTCG:PGOPTIMIZE']
    endif

    if msvc_pgo_data != ''
      msvc_pgo_link_args += ['/PGD:' + msvc_pgo_data]
      msvc_pgo_summary = msvc_pgo_mode + '(pgd=' + msvc_pgo_data + ')'
    else
      msvc_pgo_summary = msvc_pgo_mode
    endif

    add_project_link_arguments(msvc_pgo_link_args, language: 'c')
  endif
else
  if msvc_pgo_mode != 'off'
    warning('msvc_pgo is ignored unless the C compiler is msvc or clang-cl')
  endif
endif

if get_option('debug')
  conf.set('HAVE_DEBUG', 1)
endif
if get_option('tests')
  conf.set('HAVE_TESTS', 1)
endif

if host_system == 'emscripten'
  # Emscripten -s settings below are link-time settings.
  # Keep them in link arguments to avoid compile-time warnings.
  emscripten_ldflags = ['-Wno-pthreads-mem-growth']

  if get_option('emscripten_retain_compiler_settings')
    emscripten_ldflags += ['-sRETAIN_COMPILER_SETTINGS=1']
  endif
  if get_option('emscripten_noderawfs')
    emscripten_ldflags += ['-sNODERAWFS=1']
  endif

  emscripten_stack_size = get_option('emscripten_stack_size')
  emscripten_ldflags += ['-sSTACK_SIZE=@0@'.format(emscripten_stack_size)]

  emscripten_initial_memory = get_option('emscripten_initial_memory')
  emscripten_ldflags += [
    '-sINITIAL_MEMORY=@0@'.format(emscripten_initial_memory)
  ]

  emscripten_environment = get_option('emscripten_environment')
  if emscripten_environment != ''
    emscripten_ldflags += ['-sENVIRONMENT=@0@'.format(emscripten_environment)]
  endif

  if get_option('emscripten_allow_memory_growth')
    emscripten_ldflags += ['-sALLOW_MEMORY_GROWTH=1']
  endif
  if get_option('emscripten_single_file')
    emscripten_ldflags += ['-sSINGLE_FILE=1']
  endif
  if get_option('emscripten_wasm_bigint')
    emscripten_ldflags += ['-sWASM_BIGINT=1']
  endif
  if not get_option('emscripten_assertions')
    emscripten_ldflags += ['-sASSERTIONS=0']
  endif

  add_project_link_arguments(emscripten_ldflags, language: 'c')
endif

img2sixel_opt = get_option('img2sixel')
if img2sixel_opt.enabled()
  conf.set('HAVE_IMG2SIXEL', 1)
endif

sixel2png_opt = get_option('sixel2png')
if sixel2png_opt.enabled()
  conf.set('HAVE_SIXEL2PNG', 1)
endif

shebang_file = get_option('shebang_file')
shebang_helper = join_paths(meson.project_source_root(),
                            'build-aux',
                            'sixel-apply-shebang.sh')
shebang_enabled = shebang_file != ''
shebang_targets = []

# bookkeeping for configuration summary
loaders = ['stb_image']
net = []
have_winpthread = false
have_python = false
have_wic = false
have_wiccodec = false
have_quicklook_extension = false
quicklook_extension_frameworks = []
quicklook_extension_register_frameworks = []
quicklook_extension_message = 'no'
quicklook_os_supported = true
quicklook_os_reason = 'requires macOS 10.15+'
cpu = host_machine.cpu_family()

clipboard_dep = []
have_clipboard_macos = false
have_clipboard_windows = false
have_clipboard_linux = false

has_func_attr_deprecated = false
has_var_attr_deprecated = false

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

#flags = cc.get_supported_arguments([
#    '-Wformat=2',
#    '-fno-emulated-tls',
#    '-Bsymbolic',
#])

base_flags = ['-Wformat=2']
if host_system != 'openbsd'
  base_flags += ['-fno-emulated-tls']
endif

sym_flags = []
if host_system != 'openbsd'
  sym_flags += ['-Bsymbolic']
endif

flags = cc.get_supported_arguments(base_flags + sym_flags)
add_project_arguments(flags, language: 'c')

foreach f: flags
  if f == '-fno-emulated-tls'
    conf.set('HAVE_FNO_EMULATED_TLS', 1)
  elif f == '-Bsymbolic'
    conf.set('HAVE_BSYMBOLIC', 1)
  endif
endforeach

wflags = cc.get_supported_arguments([
    '-Wdeprecated-declarations',
    '-Wunused-but-set-variable',
    '-Wclobbered',
    '-Wformat-nonliteral'
])
foreach f: wflags
  if f == '-Wdeprecated-declarations'
    conf.set('HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS', 1)
  elif f == '-Wunused-but-set-variable'
    conf.set('HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE', 1)
  elif f == '-Wclobbered'
    conf.set('HAVE_DIAGNOSTIC_CLOBBERED', 1)
  elif f == '-Wformat-nonliteral'
    conf.set('HAVE_DIAGNOSTIC_FORMAT_NONLITERAL', 1)
  endif
endforeach

# Enable warnings-as-errors only in CI so local builds stay flexible.
ci_probe = find_program('python3', 'python', required: true)
ci_env = run_command(
  ci_probe,
  '-c',
  'import os, sys; sys.stdout.write(os.getenv("CI", ""))',
  check: false
).stdout().strip()
if ci_env != ''
  werror_flags = []
  if cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
    werror_flags = cc.get_supported_arguments(['-WX'])
  else
    werror_flags = cc.get_supported_arguments(['-Werror'])
  endif
  if werror_flags.length() > 0
    add_project_arguments(werror_flags, language: 'c')
    if have_objc
      add_project_arguments(werror_flags, language: 'objc')
    endif
  endif
endif

# check common headers
std_headers = [
    'assert.h',
    'math.h',
    'ctype.h',
    'memory.h',
    'unistd.h',
    'stdint.h',
    'stddef.h',
    'stdarg.h',
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
    'sys/ttycom.h',
    'sys/wait.h',
    'spawn.h',
    'execinfo.h',
    'process.h',
    'inttypes.h',
    'stdio.h',
    'stdlib.h',
    'string.h',
    'strings.h',
    'dirent.h',
    'sys/sysctl.h',
    'intrin.h',
    'float.h',
    'windows.h',
]
foreach h : std_headers
  if cc.has_header(h)
    macro = 'HAVE_' + h.to_upper().replace('.', '_').replace('/', '_')
    conf.set(macro, 1)
  endif
endforeach

# miscellaneous functions
extra_funcs = [
  'assert',
  'malloc',
  'realloc',
  'calloc',
  'memcpy',
  'memmove',
  'memset',
  'getopt_long',
  'setmode',
  '_setmode',
  'fileno',
  '_fileno',
  '_mktemp',
  'mkstemp',
  'mktemp',
  'fopen_s',
  'fsync',
  '_commit',
  '_mkdir',
  'mkdir',
  'signal',
  'strdup',
  'strtoul',
  'calloc',
  'clearerr',
  'stat',
  'setjmp',
  'longjmp',
  'strerror',
  'strerror_r',
  '_strerror_r',
  'strcpy',
  'strcpy_s',
  '_isatty',
  'isatty',
  '_sopen_s',
  '_open',
  '_write',
  '_close',
  'open',
  'sscanf_s',
  'strncmp',
  'tolower',
  '_stricmp',
  '_strcmpi',
  'strcasecmp',
  'ldiv',
  'floor',
  'pow',
  'select',
  '_fullpath',
  '_realpath',
  '_dupenv_s',
  '_putenv_s',
  'setenv',
  'realpath',
  'sqrt',
  'strchr',
  'strerror',
  'strstr',
  'strtol',
  'clock',
  'clock_gettime',
  'nanosleep',
  'usleep',
  'fork',
  'posix_spawnp',
  'execvp',
  '_chmod',
  'chmod',
  'fchmod',
  'tmpnam',
  'gettimeofday'
]

func_prefixes = {
  'setmode': '#include <io.h>',
  '_setmode': '#include <io.h>',
  '_sopen_s': '#include <io.h>\n#include <share.h>',
  'open': '#include <fcntl.h>',
  'clock_gettime': '#include <time.h>'
}

foreach f : extra_funcs
  prefix = func_prefixes.get(f, '')
  has_func = false
  if prefix != ''
    has_func = cc.has_function(f, prefix: prefix)
  else
    has_func = cc.has_function(f)
  endif
  if has_func
    conf.set('HAVE_' + f.to_upper().replace('.', '_'), 1)
  endif
endforeach

# The timezone parameter for gettimeofday() differs across libcs.  Probe
# the signature at configure time so our compatibility prototype matches
# the system declaration and avoids conflicts when headers are available.
if conf.has('HAVE_GETTIMEOFDAY')
  gtod_void_src = '''
    #include <sys/types.h>
    #include <sys/time.h>
    int main(void) {
      int (*probe)(struct timeval *, void *) = gettimeofday;
      return probe(0, 0);
    }
  '''
  gtod_tz_src = '''
    #include <sys/types.h>
    #include <sys/time.h>
    int main(void) {
      int (*probe)(struct timeval *, struct timezone *) = gettimeofday;
      return probe(0, 0);
    }
  '''
  if cc.compiles(gtod_void_src,
                 name: 'gettimeofday(void*)',
                 args: ['-Werror=implicit-function-declaration'])
    conf.set('HAVE_GETTIMEOFDAY_TZ_VOID', 1)
  elif cc.compiles(gtod_tz_src,
                   name: 'gettimeofday(struct timezone*)',
                   args: ['-Werror=implicit-function-declaration'])
    conf.set('HAVE_GETTIMEOFDAY_TZ_TIMEZONE', 1)
  endif
endif

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

if host_system == 'emscripten'
  if cc.has_header('emscripten.h')
    conf.set('HAVE_EMSCRIPTEN_H', 1)
    if cc.has_function('emscripten_get_compiler_setting',
        prefix: '#include <emscripten.h>')
      conf.set('HAVE_EMSCRIPTEN_GET_COMPILER_SETTING', 1)
    endif
  endif
endif

have_freedesktop_thumbnailing_prereqs = (
  conf.has('HAVE_UNISTD_H') and
  conf.has('HAVE_SYS_WAIT_H') and
  conf.has('HAVE_FORK') and
  not conf.has('HAVE_EMSCRIPTEN_H')
)

file_mime_option = ''
file_prog = find_program('file', required: false)
mime_fixture = join_paths(meson.project_source_root(),
                          'tests/data/inputs/formats/rgba.png')
if file_prog.found()
  foreach candidate : ['--mime-type', '-I', '-i']
    mime_probe = run_command(file_prog, candidate, mime_fixture,
                             check: false)
    if mime_probe.returncode() == 0
      mime_text = mime_probe.stdout().strip()
      if (mime_text.startswith('image/') or
          mime_text.contains(': image/') or
          mime_text.contains(' image/'))
        file_mime_option = candidate
        break
      endif
    endif
  endforeach
endif

if have_freedesktop_thumbnailing_prereqs and file_mime_option != ''
  conf.set('HAVE_FREEDESKTOP_THUMBNAILING', 1)
endif

conf.set_quoted('SIXEL_FILE_MIME_OPTION', file_mime_option,
                description: 'file(1) option used for MIME probing')

abort_trace_opt = get_option('abort_trace')
abort_trace_enabled = not abort_trace_opt.disabled()
if host_system == 'emscripten'
  if abort_trace_opt.enabled()
    error('abort_trace=enabled is not supported on emscripten')
  endif
  abort_trace_enabled = false
elif host_system == 'windows'
  if abort_trace_opt.enabled()
    error('abort_trace=enabled is not supported on this Windows environment')
  endif
  abort_trace_enabled = false
endif
abort_trace_deps = []
if abort_trace_enabled
  conf.set('SIXEL_ENABLE_ABORT_TRACE', 1)
  execinfo_dep = cc.find_library('execinfo', required: false)
  if execinfo_dep.found()
    abort_trace_deps += execinfo_dep
  endif
  execinfo_deps = []
  if execinfo_dep.found()
    execinfo_deps = [execinfo_dep]
  endif
  if cc.has_header('execinfo.h')
    if cc.has_function('backtrace',
        prefix: '#include <execinfo.h>',
        dependencies: execinfo_deps)
      conf.set('HAVE_BACKTRACE', 1)
    endif
    if cc.has_function('backtrace_symbols_fd',
        prefix: '#include <execinfo.h>',
        dependencies: execinfo_deps)
      conf.set('HAVE_BACKTRACE_SYMBOLS_FD', 1)
    endif
  endif
  if host_system == 'windows'
    dbghelp_dep = cc.find_library('dbghelp',
      required: abort_trace_opt.enabled())
    if dbghelp_dep.found()
      abort_trace_deps += dbghelp_dep
      conf.set('HAVE_DBGHELP', 1)
    elif abort_trace_opt.enabled()
      error('abort_trace=enabled requires dbghelp on Windows')
    endif
  endif
endif

deps = []
reqs = []
link_libs = []

m_dep = cc.find_library('m', required: false)
if m_dep.found()
  link_libs += m_dep
  deps += m_dep
  conf.set('HAVE_LIBM', 1)
endif

png_opt = get_option('png')
png_dep = disabler()

if not png_opt.disabled()
  if host_system == 'emscripten'
    emscripten_png_flag = '-sUSE_LIBPNG=1'
    emscripten_png_supported = cc.links('int main(void){return 0;}',
                                         args: [emscripten_png_flag],
                                         name: 'emscripten use libpng')
    if emscripten_png_supported
      png_dep = declare_dependency(compile_args: [emscripten_png_flag],
                                   link_args: [emscripten_png_flag])
    elif png_opt.enabled()
      error('png=enabled requires Emscripten support for -sUSE_LIBPNG=1')
    endif
  else
    png_dep = dependency('libpng', required: false, method: 'pkg-config')
    if not png_dep.found()
      png_dep = dependency('libpng16', required: false, method: 'pkg-config')
    endif
    if not png_dep.found()
      png_dep = dependency('PNG', required: png_opt.enabled(), method: 'cmake')
    endif
    if not png_dep.found() and png_opt.enabled()
      error('png=enabled requires libpng/libpng16 or CMake PNG package')
    endif
  endif
endif

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

tiff_opt = get_option('tiff')
tiff_dep = disabler()

if not tiff_opt.disabled()
  if host_system == 'emscripten'
    emscripten_tiff_flag = '-sUSE_LIBTIFF=1'
    emscripten_tiff_supported = cc.links('int main(void){return 0;}',
                                          args: [emscripten_tiff_flag],
                                          name: 'emscripten use libtiff')
    if emscripten_tiff_supported
      tiff_dep = declare_dependency(compile_args: [emscripten_tiff_flag],
                                    link_args: [emscripten_tiff_flag])
    elif tiff_opt.enabled()
      error('tiff=enabled requires Emscripten support for -sUSE_LIBTIFF=1')
    endif
  else
    tiff_dep = dependency('libtiff-4', required: false, method: 'pkg-config')
    if not tiff_dep.found()
      tiff_dep = cc.find_library('tiff', required: tiff_opt.enabled())
    endif
    if not tiff_dep.found() and tiff_opt.enabled()
      error('tiff=enabled requires libtiff')
    endif
  endif
endif

if tiff_dep.found()
  deps += tiff_dep
  reqs += tiff_dep
  conf.set('HAVE_LIBTIFF', 1)
  loaders += 'tiff'
endif

jpeg_opt = get_option('jpeg')
jpeg_dep = disabler()

if not jpeg_opt.disabled()
  if host_system == 'emscripten'
    emscripten_jpeg_flag = '-sUSE_LIBJPEG=1'
    emscripten_jpeg_supported = cc.links('int main(void){return 0;}',
                                          args: [emscripten_jpeg_flag],
                                          name: 'emscripten use libjpeg')
    if emscripten_jpeg_supported
      jpeg_dep = declare_dependency(compile_args: [emscripten_jpeg_flag],
                                    link_args: [emscripten_jpeg_flag])
    elif jpeg_opt.enabled()
      error('jpeg=enabled requires Emscripten support for -sUSE_LIBJPEG=1')
    endif
  else
    jpeg_dep = dependency('libjpeg',
                          required: false,
                          method: 'pkg-config')
    if not jpeg_dep.found()
      jpeg_dep = dependency('libjpeg-turbo',
                            required: false,
                            method: 'pkg-config')
    endif
    if not jpeg_dep.found()
      jpeg_dep = dependency('JPEG',
                            required: jpeg_opt.enabled(),
                            method: 'cmake')
    endif
    if not jpeg_dep.found()
      jpeg_dep = cc.find_library('jpeg',
                                 required: jpeg_opt.enabled())
    endif
    if not jpeg_dep.found() and jpeg_opt.enabled()
      error('jpeg=enabled requires libjpeg, libjpeg-turbo, ' +
            'CMake JPEG package, or a jpeg library')
    endif
  endif
endif

if jpeg_dep.found()
  deps += jpeg_dep
  reqs += jpeg_dep
  conf.set('HAVE_JPEG', 1)
  loaders += 'jpeg'
endif

webp_opt = get_option('webp')
webp_dep = disabler()
webpdemux_dep = disabler()

if not webp_opt.disabled()
  if host_system == 'emscripten'
    emscripten_webp_flag = '-sUSE_LIBWEBP=1'
    emscripten_webp_supported = cc.links('int main(void){return 0;}',
                                          args: [emscripten_webp_flag],
                                          name: 'emscripten use libwebp')
    if emscripten_webp_supported
      webp_dep = declare_dependency(compile_args: [emscripten_webp_flag],
                                    link_args: [emscripten_webp_flag])
    elif webp_opt.enabled()
      error('webp=enabled requires Emscripten support for -sUSE_LIBWEBP=1')
    endif
  else
    webp_dep = dependency('libwebp', required: false, method: 'pkg-config')
    webpdemux_dep = dependency('libwebpdemux',
                               required: false,
                               method: 'pkg-config')
    if not webp_dep.found()
      webp_dep = cc.find_library('webp', required: webp_opt.enabled())
    endif
    if not webpdemux_dep.found()
      webpdemux_dep = cc.find_library('webpdemux',
                                      required: webp_opt.enabled())
    endif
    if webp_dep.found() and webpdemux_dep.found()
      webp_dep = declare_dependency(dependencies: [webp_dep, webpdemux_dep])
    else
      webp_dep = disabler()
    endif
    if not webp_dep.found() and webp_opt.enabled()
      error('webp=enabled requires libwebp and libwebpdemux')
    endif
  endif
endif

if webp_dep.found()
  deps += webp_dep
  reqs += webp_dep
  conf.set('HAVE_WEBP', 1)
  loaders += 'webp'
endif

librsvg_opt = get_option('librsvg')
librsvg_dep = disabler()
if not librsvg_opt.disabled()
  librsvg_dep = dependency('librsvg-2.0',
                           version: '>=2.46',
                           required: false,
                           method: 'pkg-config')
  cairo_dep = dependency('cairo', required: false, method: 'pkg-config')
  if librsvg_dep.found() and cairo_dep.found()
    librsvg_dep = declare_dependency(
      dependencies: [librsvg_dep, cairo_dep])
  else
    librsvg_dep = disabler()
  endif
  if not librsvg_dep.found() and librsvg_opt.enabled()
    error('librsvg=enabled requires librsvg-2.0 >= 2.46 and cairo')
  endif
endif

if librsvg_dep.found()
  deps += librsvg_dep
  reqs += librsvg_dep
  conf.set('HAVE_LIBRSVG', 1)
  loaders += 'librsvg'
endif

curl_dep = dependency('libcurl', required: get_option('curl'))
if curl_dep.found()
  deps += curl_dep
  reqs += curl_dep
  conf.set('HAVE_LIBCURL', 1)
  net += 'libcurl'
endif

gdk_dep = dependency('gdk-pixbuf-2.0', required: false, method: 'pkg-config')
gdk_opt = get_option('gdk_pixbuf2')
if not gdk_opt.disabled()
  if not gdk_dep.found()
    gdk_dep = dependency('gdk-pixbuf-2.0',
                         required: gdk_opt.enabled(),
                         method: 'pkg-config')
  endif
  if not gdk_dep.found()
    vcpkg_root = get_env('VCPKG_ROOT', '')
    vcpkg_triplet = get_env('VCPKG_DEFAULT_TRIPLET', '')

    gdk_incdirs = []
    gdk_libdirs = []

    if vcpkg_root != '' and vcpkg_triplet != ''
      gdk_incdirs += include_directories(
        join_paths(vcpkg_root, 'installed', vcpkg_triplet, 'include'))
      gdk_libdirs += join_paths(vcpkg_root, 'installed', vcpkg_triplet, 'lib')
    endif

    gdk_lib = cc.find_library('gdk_pixbuf-2.0',
                               dirs: gdk_libdirs,
                               required: gdk_opt.enabled())

    if gdk_lib.found()
      gdk_dep = declare_dependency(link_with: gdk_lib,
                                   include_directories: gdk_incdirs)
    endif
  endif

  if gdk_dep.found()
    deps += gdk_dep
    reqs += gdk_dep
    conf.set('HAVE_GDK_PIXBUF2', 1)
    loaders += 'gdk-pixbuf2'
  elif gdk_opt.enabled()
    error('gdk_pixbuf2=enabled requires gdk-pixbuf-2.0')
  else
    message('gdk-pixbuf-2.0 not found; gdk-pixbuf2 loader disabled')
  endif
endif

gdk_pixbuf_loader_opt = get_option('gdk_pixbuf_loader')
gdk_pixbuf_loader_deps = []
gdk_pixbuf_loader_dir = ''
gdk_pixbuf_loader_message = 'no'
gdk_pixbuf_query_loader_path = ''

gdk_loader_dep = dependency('gdk-pixbuf-2.0',
                            version: '>=2.28',
                            required: gdk_pixbuf_loader_opt,
                            method: 'pkg-config')
gmodule_loader_dep = dependency('gmodule-2.0',
                                required: gdk_pixbuf_loader_opt)

if gdk_loader_dep.found() and gmodule_loader_dep.found()
  gdk_pixbuf_binary_version = gdk_loader_dep.get_variable(
    pkgconfig: 'gdk_pixbuf_binary_version')
  gdk_pixbuf_loader_dir = gdk_loader_dep.get_variable(
    pkgconfig: 'gdk_pixbuf_moduledir',
    default_value: join_paths(get_option('libdir'),
      'gdk-pixbuf-2.0', gdk_pixbuf_binary_version, 'loaders'))
  gdk_pixbuf_query_loader_path = gdk_loader_dep.get_variable(
    pkgconfig: 'gdk_pixbuf_query_loaders',
    default_value: '')
  gdk_pixbuf_loader_deps = [gdk_loader_dep, gmodule_loader_dep]
  gdk_pixbuf_loader_message = 'yes'
elif gdk_pixbuf_loader_opt.enabled()
  error('gdk_pixbuf_loader=enabled requires gdk-pixbuf-2.0 >= 2.28 and gmodule-2.0')
else
  message('gdk-pixbuf loader disabled')
endif

gdk_pixbuf_version = ''
if gdk_dep.found()
  gdk_pixbuf_version = gdk_dep.version()
elif gdk_loader_dep.found()
  gdk_pixbuf_version = gdk_loader_dep.version()
endif

if gdk_pixbuf_version != '' and gdk_pixbuf_version != 'unknown'
  if gdk_pixbuf_version.version_compare('>=2.44')
    conf.set('HAVE_GDK_PIXBUF_244', 1)
  endif
endif

gd_dep = dependency('gd', required: false)
if not gd_dep.found()
  gd_dep = dependency('gdlib', required: get_option('gd'))
endif
if gd_dep.found()
  deps += gd_dep
  reqs += gd_dep
  conf.set('HAVE_GD', 1)
  loaders += 'gd'
  gd_funcs = [
    'gdImageCreateFromGifPtr',
    'gdImageCreateFromPngPtr',
    'gdImageCreateFromBmpPtr',
    'gdImageCreateFromJpegPtrEx',
    'gdImageCreateFromJpegPtr',
    'gdImageCreateFromTgaPtr',
    'gdImageCreateFromWBMPPtr',
    'gdImageCreateFromTiffPtr',
    'gdImageCreateFromGdPtr',
    'gdImageCreateFromGd2Ptr',
    'gdImageCreateFromWebpPtr',
    'gdImagePaletteToTrueColor',
    'gdImageCreateFromGifAnimPtr',
  ]
  foreach f : gd_funcs
    conf.set10('HAVE_DECL_' + f.to_upper(),
               cc.has_function(f, prefix: '#include <gd.h>',
                               dependencies: gd_dep))
  endforeach
endif

wic_opt = get_option('wic')
if host_system == 'windows'
  if not wic_opt.disabled()
    have_wic_headers = cc.has_header('windows.h') and cc.has_header('wincodec.h')
    ole32_wic = cc.find_library('ole32', required: false)
    windowscodecs_wic = cc.find_library('windowscodecs', required: false)
    if have_wic_headers and ole32_wic.found() and windowscodecs_wic.found()
      conf.set('HAVE_WIC', 1)
      conf.set('HAVE_WINDOWS_H', 1)
      conf.set('HAVE_WINCODEC_H', 1)
      deps += [ole32_wic, windowscodecs_wic]
      link_libs += [ole32_wic, windowscodecs_wic]
      loaders += 'wic'
      have_wic = true
    elif wic_opt.enabled()
      error('WIC support requested, but required headers or libraries were not found')
    endif
  endif
elif wic_opt.enabled()
  error('WIC support is only available on Windows')
endif

threads_opt = get_option('threads')
threads_message = 'no'
have_threads = false
if not threads_opt.disabled()
  conf.set('SIXEL_ENABLE_THREADS', 1)
  threads_message = 'yes'
  have_threads = true
  if host_system != 'windows'
    threads_dep = dependency('threads', required: true)
    deps += threads_dep
    link_libs += threads_dep
  endif
else
  conf.set('SIXEL_ENABLE_THREADS', 0)
endif

wiccodec_opt = get_option('wiccodec')
wiccodec_message = 'no'
have_wiccodec = false
if host_system == 'windows'
  if not wiccodec_opt.disabled()
    have_wiccodec_headers = cc.has_header('windows.h') and cc.has_header('wincodec.h')
    ole32 = cc.find_library('ole32', required: false)
    oleaut32 = cc.find_library('oleaut32', required: false)
    gdi32 = cc.find_library('gdi32', required: false)
    windowscodecs = cc.find_library('windowscodecs', required: false)
    advapi32 = cc.find_library('advapi32', required: false)
    user32 = cc.find_library('user32', required: false)
    if have_wiccodec_headers \
         and ole32.found() \
         and oleaut32.found() \
         and gdi32.found() \
         and windowscodecs.found() \
         and advapi32.found() \
         and user32.found()
      conf.set('HAVE_WICCODEC', 1)
      if not have_wic
        conf.set('HAVE_WINDOWS_H', 1)
        conf.set('HAVE_WINCODEC_H', 1)
      endif
      if cc.has_header('wincodecsdk.h')
        conf.set('HAVE_WINCODECSDK_H', 1)
      endif
      if not have_wic
        deps += [ole32, oleaut32, gdi32, windowscodecs]
        link_libs += [ole32, oleaut32, gdi32, windowscodecs]
      endif
      deps += [advapi32, user32]
      link_libs += [advapi32, user32]
      have_wiccodec = true
      wiccodec_message = 'yes'
    elif wiccodec_opt.enabled()
      error('WIC codec support requested, but required headers or libraries were not found')
    endif
  endif
elif wiccodec_opt.enabled()
  error('WIC codec support is only available on Windows')
endif

if get_option('register_dll') and not have_wiccodec
  error('register_dll option requires WIC codec support')
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
    cg_dep = dependency('appleframeworks', \
                        modules: cg_modules, \
                        required: cg_opt.enabled())
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

quicklook_preview_opt = get_option('quicklook_preview')
appkit_opt = get_option('appkit')
have_quicklook_thumbnailing = false
if host_system == 'darwin'
  if not quicklook_preview_opt.disabled()
    if conf.get('HAVE_COREGRAPHICS', 0) != 1
      if quicklook_preview_opt.enabled()
        error('Quick Look preview requires CoreGraphics support')
      endif
    else
      quicklook_modules = ['QuickLook', 'CoreServices', 'ApplicationServices', 'CoreGraphics']
      quicklook_dep = dependency('appleframeworks',
        modules: quicklook_modules,
        required: quicklook_preview_opt.enabled())
      quicklook_thumb_dep = dependency('appleframeworks',
        modules: ['QuickLookThumbnailing', 'UniformTypeIdentifiers', 'Foundation'],
        required: false)
      if quicklook_dep.found()
        conf.set('HAVE_QUICKLOOK', 1)
        deps += quicklook_dep
        link_libs += quicklook_dep
        loaders += 'quicklook'
        if have_objc and quicklook_thumb_dep.found()
          thumb_test = '''
            #import <Foundation/Foundation.h>
            #import <QuickLookThumbnailing/QuickLookThumbnailing.h>
            int main(void)
            {
                @autoreleasepool {
                    NSURL *url = [NSURL fileURLWithPath:@"/"];
                    CGSize size = CGSizeMake(16.0f, 16.0f);
                    QLThumbnailGenerationRequest *request =
                        [[QLThumbnailGenerationRequest alloc]
                            initWithFileAtURL:url
                                         size:size
                                        scale:1.0f
                          representationTypes:
                              QLThumbnailGenerationRequestRepresentationTypeThumbnail];
                    [[QLThumbnailGenerator sharedGenerator] cancelRequest:request];
                    [request release];
                }
                return 0;
            }
          '''
          if objc.compiles(thumb_test, name: 'quicklook thumbnailing probe',
                           dependencies: quicklook_thumb_dep)
            have_quicklook_thumbnailing = true
            conf.set('HAVE_QUICKLOOK_THUMBNAILING', 1)
            deps += quicklook_thumb_dep
            link_libs += quicklook_thumb_dep
          endif
        endif
      elif quicklook_preview_opt.enabled()
        error('Quick Look preview requested, but required frameworks were not found')
      endif
    endif
  endif
elif quicklook_preview_opt.enabled()
  error('Quick Look preview requires macOS')
endif

quicklook_ext_opt = get_option('quicklook_extension')
if host_system == 'darwin'
  if not quicklook_ext_opt.disabled()
    quicklook_ext_dep = dependency('appleframeworks',
      modules: ['Foundation', \
                'AppKit', \
                'QuickLookUI', \
                'QuickLookThumbnailing', \
                'UniformTypeIdentifiers', \
                'CoreGraphics', \
                'ImageIO'],
      required: quicklook_ext_opt.enabled())
    quicklook_register_dep = dependency('appleframeworks',
      modules: ['Foundation', 'CoreServices', 'UniformTypeIdentifiers'],
      required: quicklook_ext_opt.enabled())
    if quicklook_ext_dep.found() and quicklook_register_dep.found()
      have_quicklook_extension = true
      quicklook_extension_frameworks = [quicklook_ext_dep]
      quicklook_extension_register_frameworks = [quicklook_register_dep]
      quicklook_extension_message = 'yes'
    elif quicklook_ext_opt.enabled()
      error('Quick Look extension requested, but required frameworks were not found')
    else
      quicklook_extension_message = 'no (frameworks unavailable)'
    endif
  endif
elif quicklook_ext_opt.enabled()
  error('Quick Look extension is only available on macOS')
endif

if host_system == 'darwin'
  if not appkit_opt.disabled()
    if not have_objc and appkit_opt.enabled()
      error('AppKit clipboard backend requires an Objective-C compiler')
    endif
    if have_objc
      clipboard_candidate = dependency('appleframeworks',
        modules: ['AppKit', 'Foundation'],
        required: appkit_opt.enabled())
      if clipboard_candidate.found()
        have_clipboard_macos = true
        clipboard_dep = [clipboard_candidate]
        conf.set('HAVE_APPKIT', 1)
        conf.set('HAVE_CLIPBOARD_MACOS', 1)
        deps += clipboard_dep
      endif
    endif
  endif

  if not have_clipboard_macos
    clipboard_candidate = dependency('appleframeworks',
      modules: ['Carbon', 'CoreGraphics', 'CoreFoundation'],
      required: false)
    if clipboard_candidate.found()
      if cc.compiles('''
          __attribute__((deprecated)) void test(void) {}
          #include <ApplicationServices/ApplicationServices.h>
          int main(void) {
              PasteboardRef pasteboard;
              OSStatus status;
              pasteboard = NULL;
              status = PasteboardCreate(kPasteboardClipboard, &pasteboard);
              if (status != noErr) {
                  return 1;
              }
              status = PasteboardSynchronize(pasteboard);
              return status != noErr;
          }
      ''')
        have_clipboard_macos = true
        clipboard_dep = [clipboard_candidate]
        conf.set('HAVE_CLIPBOARD_MACOS', 1)
        deps += clipboard_dep
      endif
    endif
  endif
elif host_system == 'windows'
  clipboard_candidate = cc.find_library('user32', required: false)
  if clipboard_candidate.found()
    have_clipboard_windows = true
    clipboard_dep = [clipboard_candidate]
    conf.set('HAVE_CLIPBOARD_WINDOWS', 1)
    deps += clipboard_dep
  endif
elif host_system == 'linux'
  have_clipboard_linux = true
  conf.set('HAVE_CLIPBOARD_LINUX', 1)
endif

# Limit x86-specific headers to x86 targets so ARM builds skip them cleanly.
if cpu == 'x86' or cpu == 'x86_64'
  if cc.has_header('immintrin.h')
    conf.set('HAVE_IMMINTRIN_H', 1)
  endif
  if cc.has_header('cpuid.h')
    conf.set('HAVE_CPUID_H', 1)
  endif
  if cc.has_header('intrin.h')
    conf.set('HAVE_INTRIN_H', 1)
  endif
endif
if cc.has_header('strings.h')
  conf.set('HAVE_STRINGS_H', 1)
endif
if cpu == 'x86' or cpu == 'x86_64'
  if cc.links('''#include <immintrin.h>
int main(void) {
  if (__builtin_cpu_supports("sse2")) {
    return 0;
  }
  return 0;
}
''', name: '__builtin_cpu_supports')
    conf.set('HAVE_BUILTIN_CPU_SUPPORTS', 1)
  endif
  if cc.links('''#include <immintrin.h>
int main(void) {
  __builtin_cpu_init();
  return 0;
}
''', name: '__builtin_cpu_init')
    conf.set('HAVE_BUILTIN_CPU_INIT', 1)
  endif
endif

xsave_probe_opt = get_option('xsave_probe')
xsave_probe_message = 'no'
have_xgetbv_intrin = false

if cpu == 'x86' or cpu == 'x86_64'
  if not xsave_probe_opt.disabled()
    if cc.has_header('immintrin.h') and cc.has_function('_xgetbv',
        prefix: '#include <immintrin.h>')
      conf.set('HAVE_XGETBV_INTRIN', 1)
      have_xgetbv_intrin = true
    endif
    if have_xgetbv_intrin
      conf.set('ENABLE_XSAVE_PROBE', 1)
      xsave_probe_message = 'yes'
    elif xsave_probe_opt.enabled()
      error('xsave probe requested but _xgetbv is unavailable')
    else
      xsave_probe_message = 'no (compiler)'
    endif
  else
    xsave_probe_message = 'disabled'
  endif
else
  xsave_probe_message = 'no (arch)'
endif

simd_opt = get_option('simd')
if simd_opt.disabled()
  simd_message = 'no'
elif host_system == 'emscripten'
  if cc.has_header('emmintrin.h')
    conf.set('HAVE_EMMINTRIN_H', 1)
    simd_args = ['-msimd128']
    if cc.has_multi_arguments(simd_args) and cc.compiles('''
        #include <emmintrin.h>
        int main(void){
          __m128i a = _mm_setzero_si128(); (void)a; return 0;
        }''', args: simd_args)
      conf.set('HAVE_SSE2', 1)
      simd_message = 'sse2'
      add_project_arguments(simd_args, language: 'c')
    else
      simd_message = 'no'
    endif
  else
    simd_message = 'no'
  endif
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
    simd_args = []
    if cc.get_argument_syntax() == 'msvc'
      simd_args = ['/arch:SSE2']
    else
      simd_args = ['-msse2']
    endif
    if cc.has_multi_arguments(simd_args) and cc.compiles('''
        #include <emmintrin.h>
        int main(void){
          __m128i a = _mm_setzero_si128(); (void)a; return 0;
        }''', args: simd_args)
      conf.set('HAVE_SSE2', 1)
      simd_message = 'sse2'
      add_project_arguments(simd_args, language: 'c')
    else
      simd_message = 'no'
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

libsixel_posix_defs = cc.get_supported_arguments([
  '-D_POSIX_C_SOURCE=200809L',
])

if have_threads
  libsixel_posix_defs += cc.get_supported_arguments(['-DSIXEL_ENABLE_THREADS=1'])
endif

loader_helper_sources = files(
  'src/allocator.c',
  'src/sleep.c',
  'src/sleep.h',
  'src/timer.c',
  'src/chunk.c',
  'src/colorspace.c',
  'src/dither-common-pipeline.c',
  'src/dither.c',
  'src/dither-fixed-8bit.c',
  'src/dither-fixed-float32.c',
  'src/dither-positional-8bit.c',
  'src/dither-positional-float32.c',
  'src/dither-varcoeff-8bit.c',
  'src/dither-varcoeff-float32.c',
  'src/logger.c',
  'src/logger.h',
  'src/frame.c',
  'src/fromgif.c',
  'src/frompnm.c',
  'src/fromsixel.c',
  'src/loader.c',
  'src/loader-gdk-pixbuf2.c',
  'src/loader-gnome-thumbnailer.c',
  'src/loader-coregraphics.c',
  'src/loader-quicklook.c',
  'src/loader-wic.c',
  'src/loader-gd.c',
  'src/loader-libpng.c',
  'src/loader-libtiff.c',
  'src/loader-builtin.c',
  'src/loader-common.c',
  'src/loader-registry.c',
  'src/compat_stub.c',
  'src/malloc_stub.c',
  'src/cpu.c',
  'src/cpu.h',
  'src/pixelformat.c',
  'src/palette.c',
  'src/palette-common-merge.c',
  'src/palette-common-snap.c',
  'src/palette-heckbert.c',
  'src/palette-kmeans.c',
  'src/lookup-common.c',
  'src/lookup-8bit.c',
  'src/lookup-float32.c',
  'src/lookup-fhedt-8bit.c',
  'src/lookup-fhedt-float32.c',
  'src/lookup-vptree-8bit.c',
  'src/lookup-vptree-float32.c',
  'src/dither-common-pipeline.h',
  'src/scale.c',
  'src/status.c',
  'src/tty.c',
  'src/threading.c',
)

if jpeg_dep.found()
  loader_helper_sources += files('src/loader-libjpeg.c')
endif

if webp_dep.found()
  loader_helper_sources += files('src/loader-libwebp.c')
endif

if have_threads
  loader_helper_sources += [
    'src/threadpool.c',
    'src/decoder-parallel.c',
  ]
endif

if have_quicklook_thumbnailing
  loader_helper_sources += files('src/quicklook_thumbnailing.m')
endif
loader_helpers = static_library('sixel_loader_helpers',
  loader_helper_sources,
  include_directories: project_inc,
  dependencies: deps,
  c_args: libsixel_posix_defs,
  pic: true,
  build_by_default: false)
loader_helpers_dep = declare_dependency(link_with: loader_helpers,
  include_directories: project_inc,
  dependencies: deps)

amalgamated_lib_enabled = get_option('amalgamated_lib')
amalgamated_tools_enabled = get_option('amalgamated_tools')
amalgamated_sources_enabled = amalgamated_lib_enabled or amalgamated_tools_enabled

bash_completion_opt = get_option('bashcompletiondir')
bash_completion_dir = ''
if bash_completion_opt != 'disabled'
  if bash_completion_opt == '' or bash_completion_opt == 'auto'
    prefix_dir = get_option('prefix')
    datadir_opt = get_option('datadir')
    sysconfdir_opt = get_option('sysconfdir')
    bash_completion_v2_candidates = [
      join_paths(prefix_dir, datadir_opt, 'bash-completion', 'completions'),
      '/usr/share/bash-completion/completions',
      '/usr/local/share/bash-completion/completions'
    ]
    bash_completion_v1_candidates = [
      join_paths(prefix_dir, sysconfdir_opt, 'bash_completion.d'),
      '/etc/bash_completion.d',
      '/usr/local/etc/bash_completion.d'
    ]
    foreach candidate : bash_completion_v2_candidates
      if fs.exists(candidate)
        bash_completion_dir = candidate
        break
      endif
    endforeach
    if bash_completion_dir == ''
      foreach candidate : bash_completion_v1_candidates
        if fs.exists(candidate)
          bash_completion_dir = candidate
          break
        endif
      endforeach
    endif
    if bash_completion_dir == ''
      bash_completion_dir = bash_completion_v2_candidates[0]
    endif
  else
    bash_completion_dir = bash_completion_opt
  endif
endif

zsh_completion_opt = get_option('zshcompletiondir')
zsh_completion_dir = ''
if zsh_completion_opt != 'disabled'
  if zsh_completion_opt == '' or zsh_completion_opt == 'auto'
    prefix_dir = get_option('prefix')
    datadir_opt = get_option('datadir')
    zsh_vendor_candidates = [
      join_paths(prefix_dir, datadir_opt, 'zsh', 'vendor-completions'),
      '/usr/share/zsh/vendor-completions',
      '/usr/local/share/zsh/vendor-completions'
    ]
    zsh_site_candidates = [
      join_paths(prefix_dir, datadir_opt, 'zsh', 'site-functions'),
      '/usr/share/zsh/site-functions',
      '/usr/local/share/zsh/site-functions'
    ]
    foreach candidate : zsh_vendor_candidates
      if fs.exists(candidate)
        zsh_completion_dir = candidate
        break
      endif
    endforeach
    if zsh_completion_dir == ''
      foreach candidate : zsh_site_candidates
        if fs.exists(candidate)
          zsh_completion_dir = candidate
          break
        endif
      endforeach
    endif
    if zsh_completion_dir == ''
      zsh_completion_dir = zsh_vendor_candidates[0]
    endif
  else
    zsh_completion_dir = zsh_completion_opt
  endif
endif

thumbnailer_message = 'no'
thumbnailer_opt = get_option('thumbnailer_command')
thumbnailer_install = false
sixel2png_opt = get_option('sixel2png')
update_mime_database = find_program('update-mime-database', required : false)

if sixel2png_opt.disabled()
  thumbnailer_message = 'no (requires sixel2png)'
elif thumbnailer_opt.disabled()
  thumbnailer_message = 'no'
else
  datadir_opt = get_option('datadir')
  datadir_path = join_paths(get_option('prefix'), datadir_opt)
  xdg_data_dirs = '/usr/local/share:/usr/share'
  thumbnailer_candidates = []
  foreach xdg_dir : xdg_data_dirs.split(':')
    if xdg_dir != ''
      thumbnailer_candidates += [join_paths(xdg_dir, 'thumbnailers')]
    endif
  endforeach
  thumbnailer_candidates += [join_paths(datadir_path, 'thumbnailers')]
  have_thumbnailer_dir = false
  foreach candidate : thumbnailer_candidates
    if fs.exists(candidate)
      have_thumbnailer_dir = true
      break
    endif
  endforeach
  if thumbnailer_opt.enabled()
    thumbnailer_install = true
  elif thumbnailer_opt.auto() and have_thumbnailer_dir
    thumbnailer_install = true
  endif

  if thumbnailer_install
    configure_file(
      input: 'tools/sixel-thumbnailer',
      output: 'sixel-thumbnailer',
      copy: true,
      install: true,
      install_dir: get_option('bindir')
    )
    install_data('tools/libsixel-thumbnailer.thumbnailer',
                 install_dir: join_paths(get_option('datadir'), 'thumbnailers'))
    install_data('tools/sixel.xml',
                 install_dir: join_paths(get_option('datadir'), 'mime', 'packages'))
    if update_mime_database.found()
      update_mime_script = join_paths(meson.project_source_root(), 'tools', 'update-mime-database.sh')
      meson.add_install_script(update_mime_script, update_mime_database.path(), join_paths(get_option('prefix'), get_option('datadir')), 'install', skip_if_destdir: true)
    endif
    thumbnailer_message = 'yes'
  elif thumbnailer_opt.auto()
    thumbnailer_message = 'no (thumbnailer dir not found)'
  else
    thumbnailer_message = 'no'
  endif
endif

py_opt = get_option('python')
if get_option('default_library') == 'static' and py_opt.enabled()
  error('python bindings require shared library builds; set -Dpython=disabled or avoid -Ddefault_library=static')
endif
py_mod = import('python')
py_required = py_opt.enabled()
py = py_mod.find_installation('python3', required: py_required)
if py.found()
  have_python = true
  python_message = 'yes: ' + py.get_install_dir()
else
  python_message = 'no'
endif

if py_opt.enabled() and not have_python
  error('Python support requires an interpreter')
endif

if py_opt.enabled()
  # Keep the Python module check on the same interpreter that will
  # execute wheel_builder.py later to avoid interpreter drift.
  py_check = run_command(py, '-c', 'import build', check: false)
  if py_check.returncode() != 0
    error('Python package support requires the build module')
  endif
  python_message = 'yes: libsixel_wheel'
endif

ruby_message = 'no'
have_ruby = false
ruby_gem_target = disabler()
ruby_gem_script = []
# Generate Ruby version file (pure Ruby bindings)
subdir('ruby/lib/libsixel')
ruby_opt = get_option('ruby')
if get_option('default_library') == 'static' and ruby_opt.enabled()
  error('ruby bindings require shared library builds; set -Druby=disabled or avoid -Ddefault_library=static')
endif
if ruby_opt.enabled()
  ruby = find_program('ruby', required: ruby_opt.enabled())
  if ruby.found()
    have_ruby = true
    ruby_gem_script = files('ruby/gem_builder.rb')
    # Generate constants from header
    constants_rb = custom_target('ruby_constants',
      input: 'include/sixel.h',
      output: 'constants.rb',
      command: [ruby, 'tools/gen_ruby_constants.rb', '@INPUT@', '@OUTPUT@'],
      build_by_default: true)
    # Install Ruby bindings if requested
    if ruby_opt.enabled()
      sitelibdir = run_command(ruby, '-rrbconfig', '-e', 'print RbConfig::CONFIG["sitelibdir"]').stdout()
      ruby_message = 'yes: ' + sitelibdir
      install_data('ruby/lib/libsixel.rb', install_dir: sitelibdir)
      install_subdir('ruby/lib/libsixel',
        install_dir: join_paths(sitelibdir, 'libsixel'),
        strip_directory: true,
        exclude_files: ['version_runtime.rb'])
      # Install generated runtime version file explicitly.
      # Keep constants.rb installation sourced from ruby/lib/libsixel/
      # because Meson install_data() in our supported version only accepts
      # source files, not custom_target outputs.
      install_data(ruby_version_runtime, install_dir: join_paths(sitelibdir, 'libsixel'))
    endif
  endif
endif

# Build the embedded completion header once so both the converter targets and
# the amalgamated translation unit can consume the same generated artifact.
sh = find_program('sh')
python3 = find_program('python3', required: true)
gen_completion = join_paths(
  meson.project_source_root(), 'tools', 'gen_completion_embed.py'
)
completion_inputs = files(
  'converters/shell-completion/bash/img2sixel',
  'converters/shell-completion/zsh/_img2sixel',
)
completion_embed = custom_target('completion_embed.h',
  input: completion_inputs,
  output: 'completion_embed.h',
  command: [
    sh, '-c',
    'set -eu; out="$1"; bash_in="$2"; zsh_in="$3"; conv_dir="$4"; ' +
    'mkdir -p "${conv_dir}"; ' +
    '"$5" "$6" "${out}" "${bash_in}" "${zsh_in}"; ' +
    'cp "${out}" "${conv_dir}/completion_embed.h"',
    'completion_embed', '@OUTPUT@', '@INPUT0@', '@INPUT1@',
    meson.project_build_root() / 'converters',
    python3, gen_completion,
  ],
  build_by_default: true,
  install: false,
)

if amalgamated_sources_enabled
  subdir('amalgamation')
endif

lsqa_bin = disabler()

subdir('include')

subdir('src')
if have_ruby
  ruby_gem_distdir = join_paths(meson.project_build_root(), 'ruby', 'dist')
  ruby_gem_target = custom_target(
    'libsixel-ruby-gem',
    input: libsixel_library,
    output: 'libsixel-ruby-gem.stamp',
    command: [
      ruby, ruby_gem_script,
      '--libpath', '@INPUT@',
      '--distdir', ruby_gem_distdir,
    ],
    build_by_default: true,
  )
endif
subdir('converters')
if have_wiccodec
  subdir('wic')
endif

if gdk_pixbuf_loader_deps.length() > 0
  subdir('gdk-pixbuf-loader')
  if gdk_pixbuf_query_loader_path != ''
    query_loader_candidates = [gdk_pixbuf_query_loader_path,
      'gdk-pixbuf-query-loaders']
  else
    query_loader_candidates = ['gdk-pixbuf-query-loaders']
  endif
  gdk_pixbuf_query_loaders = find_program(query_loader_candidates,
    required: false)
  if gdk_pixbuf_query_loaders.found()
    gdk_pixbuf_cache_script = join_paths(meson.project_source_root(),
      'tools', 'update-gdk-pixbuf-cache.sh')
    meson.add_install_script(gdk_pixbuf_cache_script,
      gdk_pixbuf_query_loaders.path(),
      skip_if_destdir: true)
  else
    message('gdk-pixbuf-query-loaders not found; skip cache update')
  endif
endif

subdir('assessment')


if have_quicklook_extension
  subdir('quicklook-extension')
endif
python_wheel_target = disabler()
if have_python
  subdir('python')
endif
subdir('examples')
subdir('images')


if get_option('tests')
  subdir('tests')
  if have_ruby
    env = environment()
    bindir = meson.current_build_dir()
    src_build_dir = join_paths(bindir, 'src')
    env.set('LD_LIBRARY_PATH', src_build_dir + ':' + env.get('LD_LIBRARY_PATH', ''))
    env.set('DYLD_LIBRARY_PATH', src_build_dir + ':' + env.get('DYLD_LIBRARY_PATH', ''))
    ruby_build_libdir = join_paths(bindir, 'ruby', 'lib')
    test('ruby-bindings', ruby,
         args: ['-I', ruby_build_libdir, '-I', 'ruby/lib', '-I', 'ruby/test', 'ruby/test/test_libsixel.rb'],
         env: env)
  endif
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

bash_completion_message = 'disabled'
if bash_completion_dir != ''
  bash_completion_message = bash_completion_dir
endif

zsh_completion_message = 'disabled'
if zsh_completion_dir != ''
  zsh_completion_message = zsh_completion_dir
endif

shebang_message = 'disabled'
if shebang_file != ''
  shebang_message = shebang_file
endif

webp_message = webp_dep.found() ? 'yes' : 'no'
tiff_message = tiff_dep.found() ? 'yes' : 'no'

message('')
message('libsixel was configured as follows')
message('')
message('       Loader component        : ' + loaders_summary)
message('       Network access          : ' + net_summary)
message('       libwinpthread           : ' + (have_winpthread ? 'yes' : 'no'))
message('       internal threads        : ' + threads_message)
message('       pkg-config dir          : ' + pkgconfig_dir)
message('       Bash completion dir     : ' + bash_completion_message)
message('       Zsh completion dir      : ' + zsh_completion_message)
message('       QuickLook extension     : ' + quicklook_extension_message)
message('       WIC codec               : ' + wiccodec_message)
message('       python package          : ' + python_message)
message('       ruby bindings           : ' + ruby_message)
message('       gdk-pixbuf SIXEL loader : ' + gdk_pixbuf_loader_message)
message('       Thumbnailer command     : ' + thumbnailer_message)
message('       libtiff                 : ' + tiff_message)
message('       libwebp                 : ' + webp_message)
message('       Shebang template        : ' + shebang_message)
message('       SIMD                    : ' + simd_message)
message('       XGETBV probe            : ' + xsave_probe_message)
message('       msvc pgo                : ' + msvc_pgo_summary)
message('       gcov                    : ' + (get_option('gcov') ? 'yes' : 'no'))
message('       debugging               : ' + (get_option('debug') ? 'yes' : 'no'))
message('       tests                   : ' + (get_option('tests') ? 'yes' : 'no'))
message('')
# vim: set et ts=2:
