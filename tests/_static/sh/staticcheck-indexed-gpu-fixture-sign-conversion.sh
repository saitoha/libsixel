#!/bin/sh
# Compile the indexed GPU fixture with implicit sign changes as errors.
set -eux
echo '1..1'
set -v

src_root=$1
build_root=$2
compiler=$(command -v clang || command -v gcc) || {
    echo 'ok 1 # SKIP Clang and GCC are unavailable'
    exit 0
}
test -f "$build_root/config.h" &&
    test -f "$build_root/include/sixel.h" &&
    test -f "$build_root/include/6cells.h" || {
    echo 'ok 1 # SKIP configured headers are unavailable'
    exit 0
}
"$compiler" -std=c99 -fsyntax-only -Werror=sign-conversion -DHAVE_CONFIG_H \
    -I"$build_root" -I"$build_root/include" -I"$src_root" -I"$src_root/include" \
    "$src_root/tests/encoder/indexed/0184_core_gpu_body.c" || {
    echo 'not ok 1 - indexed GPU fixture conversions preserve signedness'
    exit 0
}
echo 'ok 1 - indexed GPU fixture conversions preserve signedness'
exit 0
