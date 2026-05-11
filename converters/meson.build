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

local_inc = include_directories('.')

# for build_rpath
src_build_dir = join_paths(meson.project_build_root(), 'src')

img2sixel_opt = get_option('img2sixel')
sixel2png_opt = get_option('sixel2png')
emscripten_split_mode = host_system == 'emscripten' and not get_option('emscripten_single_file')
emscripten_tool_install_dir = get_option('bindir')
if emscripten_split_mode
  emscripten_tool_install_dir = join_paths(get_option('libexecdir'), 'libsixel')
  emscripten_launcher_conf = configuration_data()
  emscripten_launcher_libexecdir = get_option('libexecdir')
  if not emscripten_launcher_libexecdir.startswith('/')
    emscripten_launcher_libexecdir = join_paths(get_option('prefix'),
                                                emscripten_launcher_libexecdir)
  endif
  emscripten_launcher_conf.set('prefix', get_option('prefix'))
  emscripten_launcher_conf.set('exec_prefix', get_option('prefix'))
  emscripten_launcher_conf.set('libexecdir', emscripten_launcher_libexecdir)
  emscripten_sidecar_install_dir = join_paths(get_option('libexecdir'), 'libsixel')
  emscripten_sidecar_installer = find_program(
    join_paths(meson.project_source_root(),
               'build-aux',
               'install-emscripten-sidecar.sh')
  )
endif

img2sixel_deps = []
if img2sixel_opt.enabled()
  img2sixel_core_sources = ['img2sixel.c', 'completion_utils.c', 'compat.c', 'path.c', completion_embed]
  img2sixel_core_sources += sources_common + aborttrace_sources
  if amalgamated_tools_enabled
    img2sixel_exe = executable('img2sixel',
      [amalgamation_out],
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H'],
      objc_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                        '-DHAVE_COMPLETION_EMBED_H'],
      include_directories: [project_inc, local_inc],
      dependencies: deps + aborttrace_deps_local,
      install: true,
      install_dir: emscripten_tool_install_dir)
  else
    img2sixel_exe = executable('img2sixel',
      img2sixel_core_sources,
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H'],
      objc_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                        '-DHAVE_COMPLETION_EMBED_H'],
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: [project_inc, local_inc],
      build_rpath: src_build_dir,
      install: true,
      install_dir: emscripten_tool_install_dir)
  endif
  img2sixel_deps = [img2sixel_exe]

  # The AFL persistent CLI harnesses rely on POSIX-only headers and temp-file
  # or stdio redirection APIs (`unistd.h`, `dup2`, `mkstemp`, `setenv`,
  # `unsetenv`).
  # MSVC-based environments under MSYS2 may satisfy some Unix probes while
  # still failing to compile the harness sources because `unistd.h` is absent.
  # Gate these helpers on the concrete API surface they require.
  img2sixel_cli_fuzz_supported = cc.has_header('unistd.h')
  if img2sixel_cli_fuzz_supported
    img2sixel_cli_fuzz_supported = cc.has_function('setenv',
      prefix: '#include <stdlib.h>',
      args: libsixel_posix_defs)
  endif
  if img2sixel_cli_fuzz_supported
    img2sixel_cli_fuzz_supported = cc.has_function('unsetenv',
      prefix: '#include <stdlib.h>',
      args: libsixel_posix_defs)
  endif
  if img2sixel_cli_fuzz_supported
    img2sixel_cli_fuzz_supported = cc.has_function('dup2',
      prefix: '#include <unistd.h>',
      args: libsixel_posix_defs)
  endif
  if img2sixel_cli_fuzz_supported
    img2sixel_cli_fuzz_supported = cc.has_function('mkstemp',
      prefix: '#include <stdlib.h>\n#include <unistd.h>',
      args: libsixel_posix_defs)
  endif
  if not img2sixel_cli_fuzz_supported
    message('img2sixel AFL persistent CLI harnesses are disabled: required POSIX APIs are unavailable on this toolchain')
  endif

  if img2sixel_cli_fuzz_supported
    img2sixel_fuzz_harness = executable('fuzz-img2sixel-afl-persistent',
      ['../fuzz/fuzz-img2sixel-afl-persistent.c'],
      c_args: libsixel_posix_defs,
      objc_args: libsixel_posix_defs,
      dependencies: [libsixel_dep],
      include_directories: [project_inc],
      build_rpath: src_build_dir,
      build_by_default: false,
      install: false)

    img2sixel_cli_parser_harness = executable('fuzz-img2sixel-cli-parser-afl-persistent',
      ['../fuzz/fuzz-img2sixel-cli-parser-afl-persistent.c'] + img2sixel_core_sources,
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H',
                                     '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      objc_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                        '-DHAVE_COMPLETION_EMBED_H',
                                        '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: [project_inc, local_inc],
      build_rpath: src_build_dir,
      build_by_default: false,
      install: false)

    img2sixel_cli_boundary_harness = executable('fuzz-img2sixel-cli-boundary-afl-persistent',
      ['../fuzz/fuzz-img2sixel-cli-boundary-afl-persistent.c'] + img2sixel_core_sources,
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H',
                                     '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      objc_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                        '-DHAVE_COMPLETION_EMBED_H',
                                        '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: [project_inc, local_inc],
      build_rpath: src_build_dir,
      build_by_default: false,
      install: false)

    img2sixel_cli_pathenv_harness = executable('fuzz-img2sixel-cli-pathenv-afl-persistent',
      ['../fuzz/fuzz-img2sixel-cli-pathenv-afl-persistent.c'] + img2sixel_core_sources,
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H',
                                     '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      objc_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                        '-DHAVE_COMPLETION_EMBED_H',
                                        '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: [project_inc, local_inc],
      build_rpath: src_build_dir,
      build_by_default: false,
      install: false)

    img2sixel_cli_numeric_harness = executable('fuzz-img2sixel-cli-numeric-afl-persistent',
      ['../fuzz/fuzz-img2sixel-cli-numeric-afl-persistent.c'] + img2sixel_core_sources,
      c_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                     '-DHAVE_COMPLETION_EMBED_H',
                                     '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      objc_args: libsixel_posix_defs + ['-DBUILD_IMG2SIXEL',
                                        '-DHAVE_COMPLETION_EMBED_H',
                                        '-DLIBSIXEL_IMG2SIXEL_EMBEDDED'],
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: [project_inc, local_inc],
      build_rpath: src_build_dir,
      build_by_default: false,
      install: false)

    img2sixel_deps += [
      img2sixel_fuzz_harness,
      img2sixel_cli_parser_harness,
      img2sixel_cli_boundary_harness,
      img2sixel_cli_pathenv_harness,
      img2sixel_cli_numeric_harness,
    ]

run_target('fuzz-cli-img2sixel',
  command: [sh, '-c', '''
set -eu

harness="$1"
img2sixel="$2"
source_root="$3"
build_root="$4"
fuzz_afl="${FUZZ_AFL:-afl-fuzz}"
fuzz_timeout="${FUZZ_TIMEOUT:-0}"
mode="${FUZZ_IMG2SIXEL_MODE:-persistent}"
default_input_dir="$source_root/fuzz/corpus/img2sixel-cli"
if test ! -d "$default_input_dir"; then
  default_input_dir="$source_root/images"
fi
input_dir="${FUZZ_INPUT_DIR:-$default_input_dir}"
output_dir="${FUZZ_OUTPUT_DIR:-$build_root/afl-out/img2sixel}"
base_args="${FUZZ_IMG2SIXEL_BASE_ARGS:-}"
wrapper="$source_root/tools/fuzz-img2sixel-cli.sh"
runner_script="$source_root/tools/fuzz-afl-runner.sh"

if test ! -x "$runner_script"; then
  echo "error: AFL runner wrapper is missing: $runner_script" >&2
  exit 1
fi

if test "$mode" = "legacy"; then
  if test ! -x "$img2sixel"; then
    echo "error: img2sixel binary was not found: $img2sixel" >&2
    exit 1
  fi

  if test ! -r "$wrapper"; then
    echo "error: fuzz wrapper is missing: $wrapper" >&2
    exit 1
  fi

  echo "fuzz-cli-img2sixel: mode=legacy base args='$base_args'"
  "$runner_script" "fuzz-cli-img2sixel" "$fuzz_afl" "$input_dir" "$output_dir" "$fuzz_timeout" -- env FUZZ_IMG2SIXEL="$img2sixel" FUZZ_IMG2SIXEL_BASE_ARGS="$base_args" "$wrapper" @@
  exit 0
fi

if test ! -x "$harness"; then
  echo "error: persistent harness was not found: $harness" >&2
  exit 1
fi

if test -n "$base_args"; then
  echo "fuzz-cli-img2sixel: note: FUZZ_IMG2SIXEL_BASE_ARGS is ignored in persistent mode"
fi
echo "fuzz-cli-img2sixel: mode=persistent harness='$harness'"
"$runner_script" "fuzz-cli-img2sixel" "$fuzz_afl" "$input_dir" "$output_dir" "$fuzz_timeout" -- "$harness"
''',
                'sh',
                img2sixel_fuzz_harness.full_path(),
                img2sixel_exe.full_path(),
                meson.project_source_root(),
                meson.project_build_root()],
      depends: [img2sixel_fuzz_harness, img2sixel_exe])

run_target('fuzz-cli-img2sixel-parser',
  command: [sh, '-c', '''
set -eu

harness="$1"
source_root="$2"
build_root="$3"
fuzz_afl="${FUZZ_AFL:-afl-fuzz}"
fuzz_timeout="${FUZZ_TIMEOUT:-0}"
default_input_dir="$source_root/fuzz/corpus/img2sixel-cli"
if test ! -d "$default_input_dir"; then
  default_input_dir="$source_root/images"
fi
input_dir="${FUZZ_INPUT_DIR:-$default_input_dir}"
output_dir="${FUZZ_OUTPUT_DIR:-$build_root/afl-out/img2sixel-parser}"
runner_script="$source_root/tools/fuzz-afl-runner.sh"

if test ! -x "$harness"; then
  echo "error: parser harness was not found: $harness" >&2
  exit 1
fi
if test ! -x "$runner_script"; then
  echo "error: AFL runner wrapper is missing: $runner_script" >&2
  exit 1
fi

"$runner_script" "fuzz-cli-img2sixel-parser" "$fuzz_afl" "$input_dir" "$output_dir" "$fuzz_timeout" -- "$harness"
''',
                'sh',
                img2sixel_cli_parser_harness.full_path(),
                meson.project_source_root(),
                meson.project_build_root()],
      depends: [img2sixel_cli_parser_harness])

run_target('fuzz-cli-img2sixel-boundary',
  command: [sh, '-c', '''
set -eu

harness="$1"
source_root="$2"
build_root="$3"
fuzz_afl="${FUZZ_AFL:-afl-fuzz}"
fuzz_timeout="${FUZZ_TIMEOUT:-0}"
default_input_dir="$source_root/fuzz/corpus/img2sixel-cli"
if test ! -d "$default_input_dir"; then
  default_input_dir="$source_root/images"
fi
input_dir="${FUZZ_INPUT_DIR:-$default_input_dir}"
output_dir="${FUZZ_OUTPUT_DIR:-$build_root/afl-out/img2sixel-boundary}"
runner_script="$source_root/tools/fuzz-afl-runner.sh"

if test ! -x "$harness"; then
  echo "error: boundary harness was not found: $harness" >&2
  exit 1
fi
if test ! -x "$runner_script"; then
  echo "error: AFL runner wrapper is missing: $runner_script" >&2
  exit 1
fi

"$runner_script" "fuzz-cli-img2sixel-boundary" "$fuzz_afl" "$input_dir" "$output_dir" "$fuzz_timeout" -- "$harness"
''',
                'sh',
                img2sixel_cli_boundary_harness.full_path(),
                meson.project_source_root(),
                meson.project_build_root()],
      depends: [img2sixel_cli_boundary_harness])

run_target('fuzz-cli-img2sixel-pathenv',
  command: [sh, '-c', '''
set -eu

harness="$1"
source_root="$2"
build_root="$3"
fuzz_afl="${FUZZ_AFL:-afl-fuzz}"
fuzz_timeout="${FUZZ_TIMEOUT:-0}"
default_input_dir="$source_root/fuzz/corpus/img2sixel-cli"
if test ! -d "$default_input_dir"; then
  default_input_dir="$source_root/images"
fi
input_dir="${FUZZ_INPUT_DIR:-$default_input_dir}"
output_dir="${FUZZ_OUTPUT_DIR:-$build_root/afl-out/img2sixel-pathenv}"
runner_script="$source_root/tools/fuzz-afl-runner.sh"

if test ! -x "$harness"; then
  echo "error: path/env harness was not found: $harness" >&2
  exit 1
fi
if test ! -x "$runner_script"; then
  echo "error: AFL runner wrapper is missing: $runner_script" >&2
  exit 1
fi

"$runner_script" "fuzz-cli-img2sixel-pathenv" "$fuzz_afl" "$input_dir" "$output_dir" "$fuzz_timeout" -- "$harness"
''',
                'sh',
                img2sixel_cli_pathenv_harness.full_path(),
                meson.project_source_root(),
                meson.project_build_root()],
      depends: [img2sixel_cli_pathenv_harness])

run_target('fuzz-cli-img2sixel-numeric',
  command: [sh, '-c', '''
set -eu

harness="$1"
source_root="$2"
build_root="$3"
fuzz_afl="${FUZZ_AFL:-afl-fuzz}"
fuzz_timeout="${FUZZ_TIMEOUT:-0}"
default_input_dir="$source_root/fuzz/corpus/img2sixel-cli"
if test ! -d "$default_input_dir"; then
  default_input_dir="$source_root/images"
fi
input_dir="${FUZZ_INPUT_DIR:-$default_input_dir}"
output_dir="${FUZZ_OUTPUT_DIR:-$build_root/afl-out/img2sixel-numeric}"
runner_script="$source_root/tools/fuzz-afl-runner.sh"

if test ! -x "$harness"; then
  echo "error: numeric harness was not found: $harness" >&2
  exit 1
fi
if test ! -x "$runner_script"; then
  echo "error: AFL runner wrapper is missing: $runner_script" >&2
  exit 1
fi

"$runner_script" "fuzz-cli-img2sixel-numeric" "$fuzz_afl" "$input_dir" "$output_dir" "$fuzz_timeout" -- "$harness"
''',
                'sh',
                img2sixel_cli_numeric_harness.full_path(),
                meson.project_source_root(),
                meson.project_build_root()],
      depends: [img2sixel_cli_numeric_harness])
  endif

  if shebang_enabled
    shebang_targets += [custom_target('img2sixel_shebang',
      input: img2sixel_exe,
      output: 'img2sixel.shebang.stamp',
      command: [shebang_helper, shebang_file, '@INPUT@'],
      depend_files: shebang_file,
      build_by_default: true)]
  endif
  if emscripten_split_mode
    img2sixel_launcher = configure_file(
      input: 'img2sixel-node-launcher.in',
      output: 'img2sixel-node-launcher',
      configuration: emscripten_launcher_conf
    )
    install_data(
      img2sixel_launcher,
      install_dir: get_option('bindir'),
      rename: 'img2sixel'
    )
    meson.add_install_script(
      emscripten_sidecar_installer,
      img2sixel_exe.full_path(),
      emscripten_sidecar_install_dir,
      'img2sixel'
    )
  endif
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
    sixel2png_exe = executable('sixel2png',
      [amalgamation_out],
      c_args: libsixel_posix_defs + ['-DBUILD_SIXEL2PNG'],
      objc_args: libsixel_posix_defs + ['-DBUILD_SIXEL2PNG'],
      include_directories: [project_inc, local_inc],
      dependencies: deps + aborttrace_deps_local,
      install: true,
      install_dir: emscripten_tool_install_dir)
  else
    sixel2png_exe = executable('sixel2png',
      ['sixel2png.c'] + sources_common + aborttrace_sources,
      c_args: libsixel_posix_defs + ['-DBUILD_SIXEL2PNG'],
      objc_args: libsixel_posix_defs + ['-DBUILD_SIXEL2PNG'],
      dependencies: [libsixel_dep] + aborttrace_deps_local,
      include_directories: project_inc,
      build_rpath: src_build_dir,
      install: true,
      install_dir: emscripten_tool_install_dir)
  endif
  install_man('sixel2png.1')
  if shebang_enabled
    shebang_targets += [custom_target('sixel2png_shebang',
      input: sixel2png_exe,
      output: 'sixel2png.shebang.stamp',
      command: [shebang_helper, shebang_file, '@INPUT@'],
      depend_files: shebang_file,
      build_by_default: true)]
  endif
  if emscripten_split_mode
    sixel2png_launcher = configure_file(
      input: 'sixel2png-node-launcher.in',
      output: 'sixel2png-node-launcher',
      configuration: emscripten_launcher_conf
    )
    install_data(
      sixel2png_launcher,
      install_dir: get_option('bindir'),
      rename: 'sixel2png'
    )
    meson.add_install_script(
      emscripten_sidecar_installer,
      sixel2png_exe.full_path(),
      emscripten_sidecar_install_dir,
      'sixel2png'
    )
  endif
  sixel2png_deps = [sixel2png_exe]
endif

# vim: set et ts=2:
