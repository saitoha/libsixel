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
trap 'rm -f "$config_dir/config.h"; rmdir "$config_dir"' EXIT
sed '/^#define SIXEL_ENABLE_THREADS[[:space:]]/d' \
    "$build_root/config.h" > "$config_dir/config.h" || {
    echo 'not ok 1 - cannot prepare no-threads configuration'
    exit 0
}
"$cc_bin" -std=c99 -DHAVE_CONFIG_H -fsyntax-only \
    -I"$config_dir" -I"$build_root/include" -I"$src_root/include" \
    "$src_root/src/encoder.c" || {
    echo 'not ok 1 - encoder builds with SIXEL_ENABLE_THREADS undefined'
    exit 0
}
echo 'ok 1 - encoder builds with SIXEL_ENABLE_THREADS undefined'
exit 0
