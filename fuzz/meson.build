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

fuzz_common_c_args = [
  '-O1',
  '-g',
  '-fno-omit-frame-pointer',
  '-fsanitize=fuzzer-no-link,address,undefined',
]
fuzz_common_link_args = [
  '-fsanitize=fuzzer,address,undefined',
]

fuzz_loader_builtin = executable('fuzz-loader-builtin-libfuzzer',
  'fuzz-loader-builtin-libfuzzer.c',
  include_directories: project_inc,
  dependencies: [libsixel_dep],
  c_args: fuzz_common_c_args,
  link_args: fuzz_common_link_args,
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

fuzz_struct_specs = [
  ['png-chunk', 'fuzz-loader-builtin-struct-png-chunk-libfuzzer.c'],
  ['png-apng', 'fuzz-loader-builtin-struct-png-apng-libfuzzer.c'],
  ['jpeg-icc', 'fuzz-loader-builtin-struct-jpeg-icc-libfuzzer.c'],
  ['pnm-ascii', 'fuzz-loader-builtin-struct-pnm-ascii-libfuzzer.c'],
  ['psd-resource', 'fuzz-loader-builtin-struct-psd-resource-libfuzzer.c'],
  ['gif-block', 'fuzz-loader-builtin-struct-gif-block-libfuzzer.c'],
  ['sixel-stream', 'fuzz-loader-builtin-struct-sixel-stream-libfuzzer.c'],
  ['hdr-header', 'fuzz-loader-builtin-struct-hdr-header-libfuzzer.c'],
  ['pic-structure', 'fuzz-loader-builtin-struct-pic-structure-libfuzzer.c'],
  ['bmp-dib', 'fuzz-loader-builtin-struct-bmp-dib-libfuzzer.c'],
]

fuzz_struct_push_set = [
  'png-chunk',
  'png-apng',
  'jpeg-icc',
  'pnm-ascii',
  'psd-resource',
]

fuzz_struct_full_set = [
  'png-chunk',
  'png-apng',
  'jpeg-icc',
  'pnm-ascii',
  'psd-resource',
  'gif-block',
  'sixel-stream',
  'hdr-header',
  'pic-structure',
  'bmp-dib',
]

struct_exe_by_name = {}

foreach spec : fuzz_struct_specs
  struct_name = spec[0]
  struct_src = spec[1]
  struct_exe = executable('fuzz-loader-builtin-struct-' + struct_name + '-libfuzzer',
    [struct_src, 'fuzz-loader-builtin-struct-runtime.c'],
    include_directories: project_inc,
    dependencies: [libsixel_dep],
    c_args: fuzz_common_c_args,
    link_args: fuzz_common_link_args,
    build_by_default: true,
  )

  struct_exe_by_name += {
    struct_name: struct_exe,
  }

  run_target('fuzz-run-struct-' + struct_name,
    command: [sh, '-c', '''
set -eu

target="$1"
name="$2"
source_root="$3"
corpus_base="${FUZZ_STRUCT_CORPUS:-${FUZZ_BUILTIN_CORPUS:-$source_root/images}/struct}"
args="${FUZZ_STRUCT_ARGS:-${FUZZ_BUILTIN_ARGS:-}}"
corpus="$corpus_base/$name"

mkdir -p "$corpus"
echo "fuzz-run-struct[$name]: target=$target corpus=$corpus"
if test -n "$args"; then
  echo "fuzz-run-struct[$name]: args=$args"
  # shellcheck disable=SC2086
  exec "$target" $args "$corpus"
fi

exec "$target" "$corpus"
''',
              'sh',
              struct_exe.full_path(),
              struct_name,
              meson.project_source_root()],
    depends: [struct_exe])
endforeach

run_target('fuzz-run-struct-push-set',
  command: [sh, '-c', '''
set -eu

source_root="$1"
target_png_chunk="$2"
target_png_apng="$3"
target_jpeg_icc="$4"
target_pnm_ascii="$5"
target_psd_resource="$6"
corpus_base="${FUZZ_STRUCT_CORPUS:-${FUZZ_BUILTIN_CORPUS:-$source_root/images}/struct}"
args="${FUZZ_STRUCT_ARGS:-${FUZZ_BUILTIN_ARGS:-}}"

run_one() {
  name="$1"
  target="$2"
  corpus="$corpus_base/$name"
  mkdir -p "$corpus"
  echo "fuzz-run-struct[$name]: target=$target corpus=$corpus"
  if test -n "$args"; then
    echo "fuzz-run-struct[$name]: args=$args"
    # shellcheck disable=SC2086
    "$target" $args "$corpus" || exit $?
  else
    "$target" "$corpus" || exit $?
  fi
}

run_one png-chunk "$target_png_chunk"
run_one png-apng "$target_png_apng"
run_one jpeg-icc "$target_jpeg_icc"
run_one pnm-ascii "$target_pnm_ascii"
run_one psd-resource "$target_psd_resource"
''',
            'sh',
            meson.project_source_root(),
            struct_exe_by_name['png-chunk'].full_path(),
            struct_exe_by_name['png-apng'].full_path(),
            struct_exe_by_name['jpeg-icc'].full_path(),
            struct_exe_by_name['pnm-ascii'].full_path(),
            struct_exe_by_name['psd-resource'].full_path()],
  depends: [
    struct_exe_by_name['png-chunk'],
    struct_exe_by_name['png-apng'],
    struct_exe_by_name['jpeg-icc'],
    struct_exe_by_name['pnm-ascii'],
    struct_exe_by_name['psd-resource'],
  ])

run_target('fuzz-run-struct-full-set',
  command: [sh, '-c', '''
set -eu

source_root="$1"
target_png_chunk="$2"
target_png_apng="$3"
target_jpeg_icc="$4"
target_pnm_ascii="$5"
target_psd_resource="$6"
target_gif_block="$7"
target_sixel_stream="$8"
target_hdr_header="$9"
target_pic_structure="${10}"
target_bmp_dib="${11}"
corpus_base="${FUZZ_STRUCT_CORPUS:-${FUZZ_BUILTIN_CORPUS:-$source_root/images}/struct}"
args="${FUZZ_STRUCT_ARGS:-${FUZZ_BUILTIN_ARGS:-}}"

run_one() {
  name="$1"
  target="$2"
  corpus="$corpus_base/$name"
  mkdir -p "$corpus"
  echo "fuzz-run-struct[$name]: target=$target corpus=$corpus"
  if test -n "$args"; then
    echo "fuzz-run-struct[$name]: args=$args"
    # shellcheck disable=SC2086
    "$target" $args "$corpus" || exit $?
  else
    "$target" "$corpus" || exit $?
  fi
}

run_one png-chunk "$target_png_chunk"
run_one png-apng "$target_png_apng"
run_one jpeg-icc "$target_jpeg_icc"
run_one pnm-ascii "$target_pnm_ascii"
run_one psd-resource "$target_psd_resource"
run_one gif-block "$target_gif_block"
run_one sixel-stream "$target_sixel_stream"
run_one hdr-header "$target_hdr_header"
run_one pic-structure "$target_pic_structure"
run_one bmp-dib "$target_bmp_dib"
''',
            'sh',
            meson.project_source_root(),
            struct_exe_by_name['png-chunk'].full_path(),
            struct_exe_by_name['png-apng'].full_path(),
            struct_exe_by_name['jpeg-icc'].full_path(),
            struct_exe_by_name['pnm-ascii'].full_path(),
            struct_exe_by_name['psd-resource'].full_path(),
            struct_exe_by_name['gif-block'].full_path(),
            struct_exe_by_name['sixel-stream'].full_path(),
            struct_exe_by_name['hdr-header'].full_path(),
            struct_exe_by_name['pic-structure'].full_path(),
            struct_exe_by_name['bmp-dib'].full_path()],
  depends: [
    struct_exe_by_name['png-chunk'],
    struct_exe_by_name['png-apng'],
    struct_exe_by_name['jpeg-icc'],
    struct_exe_by_name['pnm-ascii'],
    struct_exe_by_name['psd-resource'],
    struct_exe_by_name['gif-block'],
    struct_exe_by_name['sixel-stream'],
    struct_exe_by_name['hdr-header'],
    struct_exe_by_name['pic-structure'],
    struct_exe_by_name['bmp-dib'],
  ])

# vim: set et ts=2:
