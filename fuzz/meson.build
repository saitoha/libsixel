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

fuzz_link_probe_source = '''
int LLVMFuzzerTestOneInput(const unsigned char *data, unsigned long size)
{
  (void)data;
  (void)size;
  return 0;
}
'''
if not cc.links(fuzz_link_probe_source, args: required_fuzz_link_args)
  error('fuzz option requires linker support for '
        + ' '.join(required_fuzz_link_args))
endif

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
format="$3"
corpus_base="${FUZZ_BUILTIN_CORPUS:-$source_root/images}"
args="${FUZZ_BUILTIN_ARGS:-}"
corpus="$corpus_base"

if test -n "$format"; then
  corpus="$corpus_base/$format"
fi
mkdir -p "$corpus"
echo "fuzz-run-loader-builtin: target=$target corpus=$corpus"
if test -n "$args"; then
  echo "fuzz-run-loader-builtin: args=$args"
fi
if test -n "$format"; then
  echo "fuzz-run-loader-builtin: format=$format"
  export FUZZ_BUILTIN_FORCE_FORMAT="$format"
fi
if test -n "$args"; then
  # shellcheck disable=SC2086
  exec "$target" $args "$corpus"
fi

exec "$target" "$corpus"
''',
            'sh',
            fuzz_loader_builtin.full_path(),
            meson.project_source_root(),
            ''],
  depends: [fuzz_loader_builtin])

fuzz_loader_formats = [
  'png',
  'gif',
  'jpeg',
  'pnm',
  'sixel',
  'hdr',
  'psd',
  'pic',
  'bmp',
]

foreach fmt : fuzz_loader_formats
  run_target('fuzz-run-loader-builtin-' + fmt,
    command: [sh, '-c', '''
set -eu

target="$1"
source_root="$2"
format="$3"
corpus_base="${FUZZ_BUILTIN_CORPUS:-$source_root/images}"
args="${FUZZ_BUILTIN_ARGS:-}"
corpus="$corpus_base/$format"

mkdir -p "$corpus"
echo "fuzz-run-loader-builtin[$format]: target=$target corpus=$corpus"
if test -n "$args"; then
  echo "fuzz-run-loader-builtin[$format]: args=$args"
fi
export FUZZ_BUILTIN_FORCE_FORMAT="$format"
if test -n "$args"; then
  # shellcheck disable=SC2086
  exec "$target" $args "$corpus"
fi

exec "$target" "$corpus"
''',
              'sh',
              fuzz_loader_builtin.full_path(),
              meson.project_source_root(),
              fmt],
    depends: [fuzz_loader_builtin])
endforeach

run_target('fuzz-run-loader-builtin-formats',
  command: [sh, '-c', '''
set -eu

target="$1"
source_root="$2"
corpus_base="${FUZZ_BUILTIN_CORPUS:-$source_root/images}"
args="${FUZZ_BUILTIN_ARGS:-}"
formats="png gif jpeg pnm sixel hdr psd pic bmp"

for format in $formats; do
  corpus="$corpus_base/$format"
  mkdir -p "$corpus"
  echo "fuzz-run-loader-builtin[$format]: target=$target corpus=$corpus"
  if test -n "$args"; then
    echo "fuzz-run-loader-builtin[$format]: args=$args"
  fi
  if test -n "$args"; then
    # shellcheck disable=SC2086
    FUZZ_BUILTIN_FORCE_FORMAT="$format" "$target" $args "$corpus" || exit $?
  else
    FUZZ_BUILTIN_FORCE_FORMAT="$format" "$target" "$corpus" || exit $?
  fi
done
''',
            'sh',
            fuzz_loader_builtin.full_path(),
            meson.project_source_root()],
  depends: [fuzz_loader_builtin])

# vim: set et ts=2:
