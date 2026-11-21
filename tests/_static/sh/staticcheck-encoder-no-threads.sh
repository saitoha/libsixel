#!/bin/sh
# Autotools can leave SIXEL_ENABLE_THREADS undefined.
set -eux
echo '1..1'
set -v

src_root=$1
build_root=$2
cc_bin=${CC:-cc}
{ test -f "$build_root/config.h" && command -v "$cc_bin" >/dev/null 2>&1; } || {
    echo 'ok 1 # SKIP configured headers or C compiler unavailable'
    exit 0
}
config_dir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-encoder-config.XXXXXX") || {
    echo 'not ok 1 - cannot prepare no-threads configuration'
    exit 0
}
trap 'rm -f "$config_dir/config.h" "$config_dir/encoder.i"; rmdir "$config_dir"' EXIT
sed '/^#define SIXEL_ENABLE_THREADS[[:space:]]/d' \
    "$build_root/config.h" > "$config_dir/config.h" || {
    echo 'not ok 1 - cannot prepare no-threads configuration'
    exit 0
}
"$cc_bin" -std=c99 -DHAVE_CONFIG_H -E \
    -I"$config_dir" -I"$build_root/include" -I"$src_root/include" \
    "$src_root/src/encoder.c" > "$config_dir/encoder.i" || {
    echo 'not ok 1 - cannot preprocess the encoder without thread support'
    exit 0
}
# Check the selected code even on compilers that lack unreachable-code warnings.
awk '
/^sixel_encoder_frame_pipeline_init\(/ { in_init = 1 }
in_init && /sixel_(mutex_init|cond_init|palette_scheduler_init)[[:space:]]*\(/ {
    threaded = 1
}
in_init && /^}/ { in_init = 0; found = 1 }
END { exit !found || threaded }
' "$config_dir/encoder.i" || {
    echo 'not ok 1 - threadless pipeline must exclude synchronization setup'
    exit 0
}
"$cc_bin" -std=c99 -fsyntax-only "$config_dir/encoder.i" || {
    echo 'not ok 1 - encoder builds with SIXEL_ENABLE_THREADS undefined'
    exit 0
}
# Compile the registered GPU test with unused-function warnings enabled.
"$cc_bin" -std=c99 -DHAVE_CONFIG_H -fsyntax-only -Wunused-function -Werror \
    -I"$config_dir" -I"$build_root/include" -I"$src_root/include" \
    -I"$src_root" "$src_root/tests/encoder/indexed/0184_core_gpu_body.c" || {
    echo 'not ok 1 - GPU-body test helpers require thread support'
    exit 0
}
echo 'ok 1 - encoder and test build with SIXEL_ENABLE_THREADS undefined'
exit 0
