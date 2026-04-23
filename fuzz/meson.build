# SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 libsixel developers. See `AUTHORS`.
#

fuzz_link_probe_source = '''
int LLVMFuzzerTestOneInput(const unsigned char *data, unsigned long size)
{
  (void)data;
  (void)size;
  return 0;
}
'''
fuzz_sanitizers = get_option('fuzz_sanitizers')
fuzz_mode_candidates = []
if fuzz_sanitizers == 'full'
  fuzz_mode_candidates = [
    ['-fsanitize=fuzzer-no-link,address,undefined', '-fsanitize=fuzzer,address,undefined'],
    ['-fsanitize=fuzzer,address,undefined', '-fsanitize=fuzzer,address,undefined'],
  ]
elif fuzz_sanitizers == 'fuzzer'
  fuzz_mode_candidates = [
    ['-fsanitize=fuzzer-no-link', '-fsanitize=fuzzer'],
    ['-fsanitize=fuzzer', '-fsanitize=fuzzer'],
  ]
else
  fuzz_mode_candidates = [
    ['-fsanitize=fuzzer-no-link,address,undefined', '-fsanitize=fuzzer,address,undefined'],
    ['-fsanitize=fuzzer,address,undefined', '-fsanitize=fuzzer,address,undefined'],
    ['-fsanitize=fuzzer-no-link', '-fsanitize=fuzzer'],
    ['-fsanitize=fuzzer', '-fsanitize=fuzzer'],
  ]
endif

selected_fuzz_c_arg = ''
selected_fuzz_link_arg = ''
foreach mode : fuzz_mode_candidates
  fuzz_c_arg = mode[0]
  fuzz_link_arg = mode[1]
  if not cc.has_argument(fuzz_c_arg)
    continue
  endif
  if cc.links(fuzz_link_probe_source, args: [fuzz_c_arg, fuzz_link_arg])
    selected_fuzz_c_arg = fuzz_c_arg
    selected_fuzz_link_arg = fuzz_link_arg
    break
  endif
endforeach

if selected_fuzz_c_arg == ''
  error('fuzz option requires a libFuzzer-capable toolchain; '
        + 'no supported sanitizer flag combination was found '
        + '(fuzz_sanitizers=' + fuzz_sanitizers + ')')
endif
message('fuzz: using compile flag "' + selected_fuzz_c_arg
        + '" and link flag "' + selected_fuzz_link_arg + '"')

fuzz_common_c_args = [
  '-O1',
  '-g',
  '-fno-omit-frame-pointer',
  selected_fuzz_c_arg,
]
fuzz_common_link_args = [
  selected_fuzz_link_arg,
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
  'webp',
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
formats="png gif jpeg pnm sixel hdr psd pic bmp webp"

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

if conf.get('HAVE_COREGRAPHICS', 0) == 1
  fuzz_loader_coregraphics = executable('fuzz-loader-coregraphics-libfuzzer',
    ['fuzz-loader-component-libfuzzer.c'],
    include_directories: project_inc,
    dependencies: [libsixel_dep],
    c_args: fuzz_common_c_args + ['-DFUZZ_LOADER_COMPONENT_NAME="coregraphics"'],
    link_args: fuzz_common_link_args,
    build_by_default: true,
  )

  run_target('fuzz-run-loader-coregraphics',
    command: [sh, '-c', '''
set -eu

target="$1"
source_root="$2"
name="$3"
corpus_base="${FUZZ_COMPONENT_CORPUS:-${FUZZ_BUILTIN_CORPUS:-$source_root/images}/component}"
args="${FUZZ_COMPONENT_ARGS:-${FUZZ_BUILTIN_ARGS:-}}"
corpus="$corpus_base/$name"

mkdir -p "$corpus"
echo "fuzz-run-loader[$name]: target=$target corpus=$corpus"
if test -n "$args"; then
  echo "fuzz-run-loader[$name]: args=$args"
  # shellcheck disable=SC2086
  exec "$target" $args "$corpus"
fi

exec "$target" "$corpus"
''',
              'sh',
              fuzz_loader_coregraphics.full_path(),
              meson.project_source_root(),
              'coregraphics'],
    depends: [fuzz_loader_coregraphics])
endif

if conf.get('HAVE_WIC', 0) == 1
  fuzz_loader_wic = executable('fuzz-loader-wic-libfuzzer',
    ['fuzz-loader-component-libfuzzer.c'],
    include_directories: project_inc,
    dependencies: [libsixel_dep],
    c_args: fuzz_common_c_args + ['-DFUZZ_LOADER_COMPONENT_NAME="wic"'],
    link_args: fuzz_common_link_args,
    build_by_default: true,
  )

  run_target('fuzz-run-loader-wic',
    command: [sh, '-c', '''
set -eu

target="$1"
source_root="$2"
name="$3"
corpus_base="${FUZZ_COMPONENT_CORPUS:-${FUZZ_BUILTIN_CORPUS:-$source_root/images}/component}"
args="${FUZZ_COMPONENT_ARGS:-${FUZZ_BUILTIN_ARGS:-}}"
corpus="$corpus_base/$name"

mkdir -p "$corpus"
echo "fuzz-run-loader[$name]: target=$target corpus=$corpus"
if test -n "$args"; then
  echo "fuzz-run-loader[$name]: args=$args"
  # shellcheck disable=SC2086
  exec "$target" $args "$corpus"
fi

exec "$target" "$corpus"
''',
              'sh',
              fuzz_loader_wic.full_path(),
              meson.project_source_root(),
              'wic'],
    depends: [fuzz_loader_wic])
endif

fuzz_struct_specs = [
  ['png-chunk', 'fuzz-loader-builtin-struct-png-chunk-libfuzzer.c', true],
  ['png-apng', 'fuzz-loader-builtin-struct-png-apng-libfuzzer.c', true],
  ['png-icc-gamma', 'fuzz-loader-builtin-struct-png-icc-gamma-libfuzzer.c', true],
  ['png-plte-trns', 'fuzz-loader-builtin-struct-png-plte-trns-libfuzzer.c', true],
  ['jpeg-icc', 'fuzz-loader-builtin-struct-jpeg-icc-libfuzzer.c', true],
  ['pnm-ascii', 'fuzz-loader-builtin-struct-pnm-ascii-libfuzzer.c', true],
  ['psd-resource', 'fuzz-loader-builtin-struct-psd-resource-libfuzzer.c', true],
  ['gif-block', 'fuzz-loader-builtin-struct-gif-block-libfuzzer.c', true],
  ['gif-lzw-codes', 'fuzz-loader-builtin-struct-gif-lzw-codes-libfuzzer.c', true],
  ['sixel-stream', 'fuzz-loader-builtin-struct-sixel-stream-libfuzzer.c', true],
  ['hdr-header', 'fuzz-loader-builtin-struct-hdr-header-libfuzzer.c', true],
  ['pic-structure', 'fuzz-loader-builtin-struct-pic-structure-libfuzzer.c', true],
  ['bmp-dib', 'fuzz-loader-builtin-struct-bmp-dib-libfuzzer.c', true],
  ['bmp-rle-bitfields', 'fuzz-loader-builtin-struct-bmp-rle-bitfields-libfuzzer.c', true],
  ['mapfile-spec', 'fuzz-loader-builtin-struct-mapfile-spec-libfuzzer.c', false],
  ['mapfile-palette', 'fuzz-loader-builtin-struct-mapfile-palette-libfuzzer.c', false],
  ['option-arglist', 'fuzz-loader-builtin-struct-option-arglist-libfuzzer.c', false],
  ['loader-order', 'fuzz-loader-builtin-struct-loader-order-libfuzzer.c', false],
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
  'png-icc-gamma',
  'png-plte-trns',
  'jpeg-icc',
  'pnm-ascii',
  'psd-resource',
  'gif-block',
  'gif-lzw-codes',
  'sixel-stream',
  'hdr-header',
  'pic-structure',
  'bmp-dib',
  'bmp-rle-bitfields',
  'mapfile-spec',
  'mapfile-palette',
  'option-arglist',
  'loader-order',
]

struct_exe_by_name = {}

foreach spec : fuzz_struct_specs
  struct_name = spec[0]
  struct_src = spec[1]
  struct_needs_runtime = spec[2]
  struct_sources = [struct_src]
  if struct_needs_runtime
    struct_sources += ['fuzz-loader-builtin-struct-runtime.c']
  endif
  struct_exe = executable('fuzz-loader-builtin-struct-' + struct_name + '-libfuzzer',
    struct_sources,
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
target_png_icc_gamma="$4"
target_png_plte_trns="$5"
target_jpeg_icc="$6"
target_pnm_ascii="$7"
target_psd_resource="$8"
target_gif_block="$9"
target_gif_lzw_codes="${10}"
target_sixel_stream="${11}"
target_hdr_header="${12}"
target_pic_structure="${13}"
target_bmp_dib="${14}"
target_bmp_rle_bitfields="${15}"
target_mapfile_spec="${16}"
target_mapfile_palette="${17}"
target_option_arglist="${18}"
target_loader_order="${19}"
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
run_one png-icc-gamma "$target_png_icc_gamma"
run_one png-plte-trns "$target_png_plte_trns"
run_one jpeg-icc "$target_jpeg_icc"
run_one pnm-ascii "$target_pnm_ascii"
run_one psd-resource "$target_psd_resource"
run_one gif-block "$target_gif_block"
run_one gif-lzw-codes "$target_gif_lzw_codes"
run_one sixel-stream "$target_sixel_stream"
run_one hdr-header "$target_hdr_header"
run_one pic-structure "$target_pic_structure"
run_one bmp-dib "$target_bmp_dib"
run_one bmp-rle-bitfields "$target_bmp_rle_bitfields"
run_one mapfile-spec "$target_mapfile_spec"
run_one mapfile-palette "$target_mapfile_palette"
run_one option-arglist "$target_option_arglist"
run_one loader-order "$target_loader_order"
''',
            'sh',
            meson.project_source_root(),
            struct_exe_by_name['png-chunk'].full_path(),
            struct_exe_by_name['png-apng'].full_path(),
            struct_exe_by_name['png-icc-gamma'].full_path(),
            struct_exe_by_name['png-plte-trns'].full_path(),
            struct_exe_by_name['jpeg-icc'].full_path(),
            struct_exe_by_name['pnm-ascii'].full_path(),
            struct_exe_by_name['psd-resource'].full_path(),
            struct_exe_by_name['gif-block'].full_path(),
            struct_exe_by_name['gif-lzw-codes'].full_path(),
            struct_exe_by_name['sixel-stream'].full_path(),
            struct_exe_by_name['hdr-header'].full_path(),
            struct_exe_by_name['pic-structure'].full_path(),
            struct_exe_by_name['bmp-dib'].full_path(),
            struct_exe_by_name['bmp-rle-bitfields'].full_path(),
            struct_exe_by_name['mapfile-spec'].full_path(),
            struct_exe_by_name['mapfile-palette'].full_path(),
            struct_exe_by_name['option-arglist'].full_path(),
            struct_exe_by_name['loader-order'].full_path()],
  depends: [
    struct_exe_by_name['png-chunk'],
    struct_exe_by_name['png-apng'],
    struct_exe_by_name['png-icc-gamma'],
    struct_exe_by_name['png-plte-trns'],
    struct_exe_by_name['jpeg-icc'],
    struct_exe_by_name['pnm-ascii'],
    struct_exe_by_name['psd-resource'],
    struct_exe_by_name['gif-block'],
    struct_exe_by_name['gif-lzw-codes'],
    struct_exe_by_name['sixel-stream'],
    struct_exe_by_name['hdr-header'],
    struct_exe_by_name['pic-structure'],
    struct_exe_by_name['bmp-dib'],
    struct_exe_by_name['bmp-rle-bitfields'],
    struct_exe_by_name['mapfile-spec'],
    struct_exe_by_name['mapfile-palette'],
    struct_exe_by_name['option-arglist'],
    struct_exe_by_name['loader-order'],
  ])

# vim: set et ts=2:
