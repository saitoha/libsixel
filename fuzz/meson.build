# SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 libsixel developers. See `AUTHORS`.
#

required_fuzz_c_args = [
  '-fsanitize=fuzzer-no-link',
  '-fsanitize=address,undefined',
]
required_fuzz_link_args = [
  '-fsanitize=fuzzer,address,undefined',
]

foreach arg : required_fuzz_c_args
  if not cc.has_argument(arg)
    error('fuzz option requires compiler support for ' + arg)
  endif
endforeach

foreach arg : required_fuzz_link_args
  if not cc.has_link_argument(arg)
    error('fuzz option requires linker support for ' + arg)
  endif
endforeach

fuzz_loader_builtin = executable('fuzz-loader-builtin-libfuzzer',
  'fuzz-loader-builtin-libfuzzer.c',
  include_directories: project_inc,
  dependencies: [libsixel_dep],
  c_args: [
    '-O1',
    '-g',
    '-fno-omit-frame-pointer',
    '-fsanitize=fuzzer-no-link,address,undefined',
  ],
  link_args: [
    '-fsanitize=fuzzer,address,undefined',
  ],
  build_by_default: true,
)

run_target('fuzz-run-loader-builtin',
  command: [sh, '-c', '''
set -eu

target="$1"
source_root="$2"
corpus="${FUZZ_BUILTIN_CORPUS:-$source_root/images}"
args="${FUZZ_BUILTIN_ARGS:-}"

mkdir -p "$corpus"
echo "fuzz-run-loader-builtin: target=$target corpus=$corpus"
if test -n "$args"; then
  echo "fuzz-run-loader-builtin: args=$args"
  # shellcheck disable=SC2086
  exec "$target" $args "$corpus"
fi

exec "$target" "$corpus"
''',
            'sh',
            fuzz_loader_builtin.full_path(),
            meson.project_source_root()],
  depends: [fuzz_loader_builtin])

# vim: set et ts=2:
