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

# Meson does not recognize pcc natively, so pcc builds use a wrapper that
# reports a GCC-like identity. Probe the predefined macro to keep pcc-specific
# workarounds explicit inside this build file.
is_pcc_backend = cc.compiles('''
  #ifndef __PCC__
  #error not pcc
  #endif
  int main(void) { return 0; }
''')

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

if is_pcc_backend
  # pcc-provided crt objects can miss .note.GNU-stack. Ask the linker to mark
  # the output stack as non-executable when the flag is supported.
  if cc.has_link_argument('-Wl,-z,noexecstack')
    add_project_link_arguments('-Wl,-z,noexecstack', language: 'c')
  endif

  # Meson's generated config.h uses #pragma once. pcc accepts it but warns on
  # every translation unit, so suppress unknown-pragma diagnostics here.
  if cc.has_argument('-Wno-unknown-pragmas')
    add_project_arguments('-Wno-unknown-pragmas', language: 'c')
  endif
endif

analyzer_summary = 'disabled'
if get_option('analyzer')
  if cc.get_id() == 'clang'
    scan_build_prog = find_program(
      'scan-build',
      'scan-build-18',
      'scan-build-17',
      'scan-build-16',
      required: true
    )
    analyzer_summary = 'clang(scan-build=' + scan_build_prog.full_path() + ')'
  elif cc.get_id() == 'gcc'
    add_project_arguments(['-O2', '-fanalyzer'], language: 'c')
    analyzer_summary = 'gcc(-O2 -fanalyzer)'
  elif cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
    add_project_arguments(['/W4', '/analyze', '/wd28253', '/wd6262'],
                          language: 'c')
    analyzer_summary = 'msvc(/W4 /analyze /wd28253 /wd6262)'
  else
    error('analyzer=true is unsupported for C compiler: ' + cc.get_id())
  endif
endif

msvc_pgo_mode = get_option('msvc_pgo')
msvc_pgo_data = get_option('msvc_pgo_data')
msvc_pgo_summary = 'off'
msvc_pgo_compile_args = []
msvc_pgo_link_args = []
if cc.get_id() == 'msvc' or cc.get_id() == 'clang-cl'
  if msvc_pgo_mode != 'off'
    # Keep PGO flags explicit for MSVC-like frontends because Meson base
    # option b_pgo currently targets GNU/LLVM-style -fprofile-* flags.
    #
    # MSVC linker instrumentation emits C4964 when no optimization flag is
    # active, and this project treats warnings as errors on CI. Force an
    # optimization level during msvc_pgo generate/use to keep instrumentation
    # valid even with buildtype=plain.
    msvc_pgo_compile_args += ['/O2', '/GL']
    if msvc_pgo_mode == 'generate'
      msvc_pgo_link_args += ['/LTCG:PGINSTRUMENT']
    elif msvc_pgo_mode == 'use'
      msvc_pgo_link_args += ['/LTCG:PGOPTIMIZE']
    endif

    # Avoid forcing a single .pgd path at project scope unless explicitly
    # requested. The global link arguments apply to every target, so sharing
    # one .pgd can cause MSVC linker contention in parallel builds.
    if msvc_pgo_data != ''
      msvc_pgo_link_args += ['/PGD:' + msvc_pgo_data]
      msvc_pgo_summary = msvc_pgo_mode + '(pgd=' + msvc_pgo_data + ')'
    else
      msvc_pgo_summary = msvc_pgo_mode + '(pgd=per-target default)'
    endif

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
fuzz_enabled = get_option('fuzz')
fuzz_sanitizers = get_option('fuzz_sanitizers')
fuzz_message = fuzz_enabled ? 'yes' : 'no'
objc_forced_off_for_fuzz = false

if fuzz_enabled
  # Keep an early compile-side sanity check, but avoid link-side checks here.
  # On clang+MSVC toolchains, cc.has_link_argument() can report false negatives
  # for sanitizer flags even when fuzz/meson.build link probes succeed.
  fuzz_compile_probe_args = []
  if fuzz_sanitizers == 'full'
    fuzz_compile_probe_args = [
      '-fsanitize=fuzzer-no-link,address,undefined',
      '-fsanitize=fuzzer,address,undefined',
    ]
  elif fuzz_sanitizers == 'fuzzer'
    fuzz_compile_probe_args = [
      '-fsanitize=fuzzer-no-link',
      '-fsanitize=fuzzer',
    ]
  else
    fuzz_compile_probe_args = [
      '-fsanitize=fuzzer-no-link,address,undefined',
      '-fsanitize=fuzzer,address,undefined',
      '-fsanitize=fuzzer-no-link',
      '-fsanitize=fuzzer',
    ]
  endif
  fuzz_compile_probe_ok = false
  foreach arg : fuzz_compile_probe_args
    if cc.has_argument(arg)
      fuzz_compile_probe_ok = true
      break
    endif
  endforeach
  if not fuzz_compile_probe_ok
    error('fuzz option requires compiler support for libFuzzer sanitizer flags'
          + ' (fuzz_sanitizers=' + fuzz_sanitizers + ')')
  endif

  if host_system == 'darwin' and have_objc
    if cc.version() != objc.version()
      warning(
        'fuzz=true on macOS detected mixed toolchains (C=' + cc.version() + ', ObjC=' + objc.version() + '). ' +
        'Disabling Objective-C integrations (Quick Look/AppKit) for this fuzz build to avoid sanitizer runtime mismatch.'
      )
      have_objc = false
      objc_forced_off_for_fuzz = true
    endif
  endif
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

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
    int main(void) { return 0; }
    #pragma GCC diagnostic pop
  ''', args: ['-Werror'])
  conf.set('HAVE_DIAGNOSTIC_WERROR_ANALYZER_FD_LEAK', 1)
endif

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
    int main(void) { return 0; }
    #pragma GCC diagnostic pop
  ''', args: ['-Werror'])
  conf.set('HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK', 1)
endif

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wanalyzer-out-of-bounds"
    int main(void) { return 0; }
    #pragma GCC diagnostic pop
  ''', args: ['-Werror'])
  conf.set('HAVE_DIAGNOSTIC_WANALYZER_OUT_OF_BOUNDS', 1)
endif

if cc.compiles('''
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
    int main(void) { return 0; }
    #pragma GCC diagnostic pop
  ''', args: ['-Werror'])
  conf.set('HAVE_DIAGNOSTIC_WANALYZER_USE_OF_UNINITIALIZED_VALUE', 1)
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

flags = cc.get_supported_arguments(base_flags)
add_project_arguments(flags, language: 'c')

if host_system != 'openbsd'
  # Use a linker-style option here. Passing raw "-Bsymbolic" as a compile
  # option can be misparsed by some compilers (for example, tcc treats it as
  # "-B <prefix>"), which breaks standard include/library path resolution.
  bsymbolic_ldflag = '-Wl,-Bsymbolic'
  if cc.has_link_argument(bsymbolic_ldflag)
    add_project_link_arguments(bsymbolic_ldflag, language: 'c')
    conf.set('HAVE_BSYMBOLIC', 1)
  endif
endif

foreach f: flags
  if f == '-fno-emulated-tls'
    conf.set('HAVE_FNO_EMULATED_TLS', 1)
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

# Keep profile output paths usable for PGO generate runs even when the
# caller exports LLVM_PROFILE_FILE into a directory that does not exist yet.
if get_option('b_pgo') == 'generate'
  llvm_profile_file = run_command(
    ci_probe,
    '-c',
    'import os, sys; sys.stdout.write(os.getenv("LLVM_PROFILE_FILE", ""))',
    check: false
  ).stdout().strip()
  if llvm_profile_file != ''
    run_command(
      ci_probe,
      '-c',
      'import os, sys; p = sys.argv[1]; d = os.path.dirname(p); ' +
      'os.makedirs(d, exist_ok=True) if d else None',
      llvm_profile_file,
      check: true
    )
  endif
endif

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
    'sys/param.h',
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
  jpeg_has_12_api = cc.links(
    '''
#include <stdio.h>
#include <jpeglib.h>
#if !defined(J12SAMPLE) || !defined(MAXJ12SAMPLE)
# error "missing J12SAMPLE/MAXJ12SAMPLE"
#endif
int main(void) {
  j_decompress_ptr cinfo = (j_decompress_ptr)0;
  J12SAMPARRAY scanlines = (J12SAMPARRAY)0;
  (void)jpeg12_read_scanlines(cinfo, scanlines, (JDIMENSION)0);
  return 0;
}
''',
    dependencies: jpeg_dep,
    name: 'libjpeg 12-bit scanline api')
  if jpeg_has_12_api
    conf.set('HAVE_JPEG12_API', 1)
  endif

  jpeg_has_16_api = cc.links(
    '''
#include <stdio.h>
#include <jpeglib.h>
#if !defined(J16SAMPLE) || !defined(MAXJ16SAMPLE)
# error "missing J16SAMPLE/MAXJ16SAMPLE"
#endif
int main(void) {
  j_decompress_ptr cinfo = (j_decompress_ptr)0;
  J16SAMPARRAY scanlines = (J16SAMPARRAY)0;
  (void)jpeg16_read_scanlines(cinfo, scanlines, (JDIMENSION)0);
  return 0;
}
''',
    dependencies: jpeg_dep,
    name: 'libjpeg 16-bit scanline api')
  if jpeg_has_16_api
    conf.set('HAVE_JPEG16_API', 1)
  endif

  jpeg_has_arith_decoder = cc.links(
    '''
#include <stdio.h>
#include <jpeglib.h>
#ifndef D_ARITH_CODING_SUPPORTED
# error "missing D_ARITH_CODING_SUPPORTED"
#endif
int main(void) {
  return 0;
}
''',
    dependencies: jpeg_dep,
    name: 'libjpeg arithmetic decode support')
  if jpeg_has_arith_decoder
    conf.set('HAVE_JPEG_ARITH_DECODER', 1)
  endif

  deps += jpeg_dep
  reqs += jpeg_dep
  conf.set('HAVE_JPEG', 1)
  loaders += 'jpeg'
endif

webp_opt = get_option('webp')
webp_dep = disabler()
webpdemux_dep = disabler()
lcms2_opt = get_option('lcms2')
lcms2_dep = disabler()
lcms2_header_found = false

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

if not lcms2_opt.disabled()
  lcms2_dep = dependency('lcms2', required: lcms2_opt.enabled(), method: 'pkg-config')
  if lcms2_dep.found()
    # Verify headers explicitly to avoid enabling lcms2 when only runtime
    # libraries are available (for example on Solaris build environments).
    lcms2_header_found = cc.has_header('lcms2.h', dependencies: lcms2_dep)
  endif
  if not lcms2_dep.found()
    lcms2_dep = cc.find_library('lcms2', required: lcms2_opt.enabled())
    if lcms2_dep.found()
      # find_library() does not validate development headers.
      lcms2_header_found = cc.has_header('lcms2.h')
      if not lcms2_header_found
        lcms2_dep = disabler()
      endif
    endif
  endif
  if lcms2_opt.enabled() and (not lcms2_dep.found() or not lcms2_header_found)
    error('lcms2=enabled requires both lcms2 library and lcms2.h header')
  endif
endif

if lcms2_dep.found() and lcms2_header_found
  deps += lcms2_dep
  reqs += lcms2_dep
  conf.set('HAVE_LCMS2', 1)
endif

librsvg_opt = get_option('librsvg')
librsvg_dep = disabler()
enable_librsvg = not librsvg_opt.disabled()
if is_pcc_backend and librsvg_opt.auto()
  # GCC/clang-oriented glib/cairo headers trigger pcc diagnostics and can
  # abort the build. Keep pcc auto builds warning-free by default.
  enable_librsvg = false
  message('pcc backend detected: disable librsvg auto loader')
endif
if enable_librsvg
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

fetch_opt = get_option('fetch')
fetch_dep = disabler()
if not fetch_opt.disabled()
  have_fetch_header = false
  if host_system == 'emscripten'
    # Emscripten does not provide BSD libfetch; use the built-in Fetch API.
    emscripten_fetch_flag = '-sFETCH=1'
    emscripten_fetch_supported = cc.links('int main(void){return 0;}',
                                          args: [emscripten_fetch_flag],
                                          name: 'emscripten fetch api')
    if emscripten_fetch_supported
      have_fetch_header = cc.has_header('emscripten/fetch.h')
      if have_fetch_header
        fetch_dep = declare_dependency(
          link_args: [emscripten_fetch_flag])
      endif
    endif
  else
    fetch_dep = dependency('libfetch',
                           required: false,
                           method: 'pkg-config')
    if fetch_dep.found()
      have_fetch_header = cc.has_header('fetch.h', dependencies: fetch_dep)
      if not have_fetch_header
        fetch_dep = disabler()
      endif
    else
      fetch_lib = cc.find_library('fetch', required: false)
      if fetch_lib.found() and cc.has_header('fetch.h')
        fetch_dep = declare_dependency(dependencies: [fetch_lib])
        have_fetch_header = true
      endif
    endif
  endif

  if fetch_opt.enabled() and not fetch_dep.found()
    error('libfetch support requested, but required headers or library were not found')
  endif
endif

if fetch_dep.found()
  deps += fetch_dep
  conf.set('HAVE_LIBFETCH', 1)
  net += 'libfetch'
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
      if not have_objc
        if quicklook_ext_opt.enabled() and not objc_forced_off_for_fuzz
          error('Quick Look extension requires an Objective-C compiler')
        endif
        if objc_forced_off_for_fuzz
          quicklook_extension_message = 'no (disabled for fuzz toolchain compatibility)'
        else
          quicklook_extension_message = 'no (Objective-C compiler unavailable)'
        endif
      else
        openurls_returns_running_application = objc.compiles('''
            #import <Foundation/Foundation.h>
            #import <AppKit/AppKit.h>
            int main(void) {
                NSURL *url = [NSURL fileURLWithPath:@"/tmp"];
                NSError *error = nil;
            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wdeprecated-declarations"
                NSRunningApplication *app = [[NSWorkspace sharedWorkspace]
                    openURLs:@[url]
                    withApplicationAtURL:url
                    options:NSWorkspaceLaunchDefault
                    configuration:@{}
                    error:&error];
            #pragma clang diagnostic pop
                return (app == nil && error != nil) ? 0 : 0;
            }
          ''',
          name: 'NSWorkspace openURLs returns NSRunningApplication *',
          dependencies: quicklook_ext_dep,
          args: ['-fobjc-arc', '-fmodules'])

        openurls_returns_bool = false
        if not openurls_returns_running_application
          openurls_returns_bool = objc.compiles('''
              #import <Foundation/Foundation.h>
              #import <AppKit/AppKit.h>
              int main(void) {
                  NSURL *url = [NSURL fileURLWithPath:@"/tmp"];
                  NSError *error = nil;
              #pragma clang diagnostic push
              #pragma clang diagnostic ignored "-Wdeprecated-declarations"
                  BOOL success = [[NSWorkspace sharedWorkspace]
                      openURLs:@[url]
                      withApplicationAtURL:url
                      options:NSWorkspaceLaunchDefault
                      configuration:@{}
                      error:&error];
              #pragma clang diagnostic pop
                  return (success || error != nil) ? 0 : 0;
              }
            ''',
            name: 'NSWorkspace openURLs returns BOOL',
            dependencies: quicklook_ext_dep,
            args: ['-fobjc-arc', '-fmodules'])
        endif

        if openurls_returns_running_application
          conf.set('HAVE_NSWORKSPACE_OPENURLS_RETURNS_RUNNING_APPLICATION', 1)
        elif openurls_returns_bool
          conf.set('HAVE_NSWORKSPACE_OPENURLS_RETURNS_BOOL', 1)
        elif quicklook_ext_opt.enabled()
          error('Unable to determine NSWorkspace openURLs return type')
        endif

        have_quicklook_extension = true
        quicklook_extension_frameworks = [quicklook_ext_dep]
        quicklook_extension_register_frameworks = [quicklook_register_dep]
        quicklook_extension_message = 'yes'
      endif
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
    if not have_objc and appkit_opt.enabled() and not objc_forced_off_for_fuzz
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
# pcc does not ship GNU x86 intrinsic headers (cpuid.h/immintrin.h), so keep
# these probes disabled there even when wrapped as a GCC frontend.
if (cpu == 'x86' or cpu == 'x86_64') and not is_pcc_backend
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
if (cpu == 'x86' or cpu == 'x86_64') and not is_pcc_backend
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

if is_pcc_backend
  xsave_probe_message = 'no (pcc)'
elif cpu == 'x86' or cpu == 'x86_64'
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
elif is_pcc_backend
  simd_message = 'no (pcc)'
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

# Meson can report false positives for a few GNU/MSVC probes when the compiler
# identity is wrapped for pcc detection.  Force these probes off so Linux+pcc
# links use the same code paths as autotools+pcc.
if is_pcc_backend and host_system != 'windows'
  foreach macro_name : [
    'HAVE_FOPEN_S',
    'HAVE_SSCANF_S',
    'HAVE_STRCPY_S',
    'HAVE__CHMOD',
    'HAVE__CLOSE',
    'HAVE__COMMIT',
    'HAVE__DUPENV_S',
    'HAVE__FILENO',
    'HAVE__FULLPATH',
    'HAVE__ISATTY',
    'HAVE__MKDIR',
    'HAVE__MKTEMP',
    'HAVE__OPEN',
    'HAVE__PUTENV_S',
    'HAVE__REALPATH',
    'HAVE__STRCMPI',
    'HAVE__STRERROR_R',
    'HAVE__STRICMP',
    'HAVE__WRITE',
    'HAVE___BUILTIN_UNREACHABLE',
  ]
    conf.set(macro_name, false)
  endforeach
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
  'src/dither-interframe-method.h',
  'src/logger.c',
  'src/logger.h',
  'src/frame.c',
  'src/fromgif.c',
  'src/fromwebp.c',
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
  'src/palette-kmeans-assign.c',
  'src/palette-kmeans-build.c',
  'src/palette-kmeans-build.h',
  'src/palette-kmedoids.c',
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

amalgamated_dither_policy_sources = files(
  'src/dither-policy-none.c',
  'src/dither-policy-fs.c',
  'src/dither-policy-atkinson.c',
  'src/dither-policy-jajuni.c',
  'src/dither-policy-stucki.c',
  'src/dither-policy-burkes.c',
  'src/dither-policy-sierra1.c',
  'src/dither-policy-sierra2.c',
  'src/dither-policy-sierra3.c',
  'src/dither-policy-a-dither.c',
  'src/dither-policy-x-dither.c',
  'src/dither-policy-bluenoise.c',
  'src/dither-policy-interframe.c',
  'src/dither-policy-lso2.c',
)

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
  py_venv_check = run_command(
    py, '-c',
    'import importlib.util,sys; missing=[m for m in ("venv","ensurepip") if importlib.util.find_spec(m) is None]; sys.exit(0 if not missing else 1)',
    check: false,
  )
  if py_venv_check.returncode() != 0
    error('Python test environment requires venv and ensurepip modules')
  endif
  python_message = 'yes: libsixel_wheel'
endif

# Configure the public header before optional language bindings.
# Ruby constant generation consumes the configured sixel.h.
subdir('include')

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
ruby = find_program('ruby', required: ruby_opt.enabled())
if ruby_opt.enabled() and ruby.found()
  ruby_fiddle_check = run_command(
    ruby, '-e',
    'begin; require "fiddle"; exit 0; rescue LoadError; exit 1; end',
    check: false,
  )
  if ruby_fiddle_check.returncode() != 0
    if ruby_opt.enabled()
      error('ruby bindings require the standard Fiddle library')
    endif
  else
    have_ruby = true
    ruby_gem_script = files('ruby/gem_builder.rb')
    # Generate Ruby constants from the configured public header.
    # The header is produced by include/meson.build via configure_file(),
    # so we must reference the generated target (sixel_h) instead of a
    # source-tree path such as include/sixel.h.
    constants_rb = custom_target('ruby_constants',
      input: sixel_h,
      output: 'constants.rb',
      command: [ruby,
        join_paths(meson.project_source_root(), 'tools', 'gen_ruby_constants.rb'),
        '@INPUT@', '@OUTPUT@'],
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

php_message = 'no'
have_php = false
php_package_target = disabler()
php_opt = get_option('php')
if get_option('default_library') == 'static' and php_opt.enabled()
  error('php bindings require shared library builds; set -Dphp=disabled or avoid -Ddefault_library=static')
endif
php = find_program('php', required: php_opt.enabled())
if php_opt.enabled() and php.found()
  php_ffi_check = run_command(php, '-m', check: false)
  php_ffi_available = false
  if php_ffi_check.returncode() == 0
    php_ffi_available = ('\n' + php_ffi_check.stdout() + '\n').contains('\nFFI\n')
  endif
  if not php_ffi_available
    error('PHP FFI extension is required (enable ext-ffi)')
  else
    have_php = true
    php_message = 'yes: libsixel-php'
  endif
endif

perl_message = 'no'
have_perl = false
perl_opt = get_option('perl')
if get_option('default_library') == 'static' and perl_opt.enabled()
  error('perl bindings require shared library builds; set -Dperl=disabled or avoid -Ddefault_library=static')
endif
perl = find_program('perl', required: perl_opt.enabled())
if perl_opt.enabled() and perl.found()
  perl_ffi_check = run_command(
    perl,
    '-MFFI::Platypus',
    '-MFFI::Platypus::Buffer',
    '-MFFI::Platypus::Closure',
    '-e',
    'exit 0',
    check: false,
  )
  if perl_ffi_check.returncode() != 0
    error('Perl bindings require FFI::Platypus, FFI::Platypus::Buffer, and FFI::Platypus::Closure')
  else
    have_perl = true
    perl_message = 'yes: perl binding tests'
  endif
endif

# Build the embedded completion header once so both the converter targets and
# the amalgamated translation unit can consume the same generated artifact.
sh = find_program('sh')
meson_prog = find_program('meson', required: true)
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

if have_wiccodec
  subdir('wic')
endif

msvc_pgo_post_wic_deps = []
if msvc_pgo_mode != 'off' and msvc_pgo_link_args.length() > 0
  # Keep WIC uninstrumented: attach MSVC PGO flags only to targets declared
  # after the WIC module instead of mutating project-wide arguments.
  msvc_pgo_post_wic_deps = [declare_dependency(
    compile_args: msvc_pgo_compile_args,
    link_args: msvc_pgo_link_args,
  )]
endif

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
      '--version', meson.project_version(),
    ],
    build_by_default: true,
  )
endif
subdir('converters')
subdir('tools')

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
if have_php
  subdir('php')
endif
subdir('examples')
subdir('images')


if get_option('tests')
  subdir('tests')
endif

run_target('installcheck',
  command: [sh, '-c', '''
set -eu

meson_bin="$1"
build_root="$2"
prefix="$3"
bindir="$4"
libexecdir="$5"
libdir="$6"
host_system="$7"
cross="$8"
tests_enabled="$9"
bin_ext="${10}"

if test "$cross" = "yes"; then
  echo "installcheck: skipped for cross builds"
  exit 0
fi

if test "$tests_enabled" != "yes"; then
  echo "installcheck: skipped because tests are disabled"
  exit 0
fi

case "$bindir" in
  /*) install_bindir="$bindir" ;;
  *) install_bindir="$prefix/$bindir" ;;
esac

case "$libexecdir" in
  /*) install_libexecdir="$libexecdir" ;;
  *) install_libexecdir="$prefix/$libexecdir" ;;
esac

case "$libdir" in
  /*) install_libdir="$libdir" ;;
  *) install_libdir="$prefix/$libdir" ;;
esac

if test ! -d "$install_bindir"; then
  echo "installcheck: installed bindir not found: $install_bindir" >&2
  echo "installcheck: run 'meson install -C $build_root' first" >&2
  exit 1
fi

if test ! -d "$install_libdir"; then
  echo "installcheck: installed libdir not found: $install_libdir" >&2
  echo "installcheck: run 'meson install -C $build_root' first" >&2
  exit 1
fi

PATH="$install_bindir:$build_root/tests:$PATH"
export PATH

if test "$host_system" = "darwin"; then
  if test -n "${DYLD_LIBRARY_PATH:-}"; then
    DYLD_LIBRARY_PATH="$install_libdir:${DYLD_LIBRARY_PATH}"
  else
    DYLD_LIBRARY_PATH="$install_libdir"
  fi
  export DYLD_LIBRARY_PATH
else
  if test -n "${LD_LIBRARY_PATH:-}"; then
    LD_LIBRARY_PATH="$install_libdir:${LD_LIBRARY_PATH}"
  else
    LD_LIBRARY_PATH="$install_libdir"
  fi
  export LD_LIBRARY_PATH
fi

export SIXEL_RUNTIME=''
export SIXEL_INSTALLCHECK=1
export SIXEL_INSTALL_BINDIR="$install_bindir"
export SIXEL_INSTALL_LIBEXECDIR="$install_libexecdir"
export SIXEL_INSTALL_LIBDIR="$install_libdir"
export SIXEL_INSTALLCHECK_IMG2SIXEL_PATH="img2sixel${bin_ext}"
export SIXEL_INSTALLCHECK_SIXEL2PNG_PATH="sixel2png${bin_ext}"
export SIXEL_INSTALLCHECK_LSQA_PATH="lsqa${bin_ext}"
export SIXEL_INSTALLCHECK_TEST_RUNNER_PATH="test_runner${bin_ext}"

if test -n "${SIXEL_INSTALLCHECK_TESTS-}"; then
  # Allow callers to run a focused subset while keeping installcheck defaults.
  # shellcheck disable=SC2086
  "$meson_bin" test -C "$build_root" --no-rebuild --suite tap --print-errorlogs --setup installcheck $SIXEL_INSTALLCHECK_TESTS
else
  "$meson_bin" test -C "$build_root" --no-rebuild --suite tap --print-errorlogs --setup installcheck
fi
''',
            'meson-installcheck',
            meson_prog,
            meson.project_build_root(),
            get_option('prefix'),
            get_option('bindir'),
            get_option('libexecdir'),
            get_option('libdir'),
            host_machine.system(),
            meson.is_cross_build() ? 'yes' : 'no',
            get_option('tests') ? 'yes' : 'no',
            (host_machine.system() == 'windows' or
             host_machine.system() == 'cygwin') ? '.exe' : ''])

if fuzz_enabled
  subdir('fuzz')
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
lcms2_message = lcms2_dep.found() ? 'yes' : 'no'

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
message('       php bindings            : ' + php_message)
message('       perl bindings           : ' + perl_message)
message('       gdk-pixbuf SIXEL loader : ' + gdk_pixbuf_loader_message)
message('       Thumbnailer command     : ' + thumbnailer_message)
message('       libtiff                 : ' + tiff_message)
message('       libwebp                 : ' + webp_message)
message('       lcms2                   : ' + lcms2_message)
message('       Shebang template        : ' + shebang_message)
message('       SIMD                    : ' + simd_message)
message('       XGETBV probe            : ' + xsave_probe_message)
message('       analyzer                : ' + analyzer_summary)
message('       msvc pgo                : ' + msvc_pgo_summary)
message('       gcov                    : ' + (get_option('gcov') ? 'yes' : 'no'))
message('       debugging               : ' + (get_option('debug') ? 'yes' : 'no'))
message('       fuzz targets            : ' + fuzz_message)
message('       tests                   : ' + (get_option('tests') ? 'yes' : 'no'))
message('')
# vim: set et ts=2:
