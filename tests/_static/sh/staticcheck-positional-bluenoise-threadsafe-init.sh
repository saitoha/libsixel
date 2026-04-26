#!/bin/sh
# Ensure positional bluenoise paths do not read shared config before init.

set -eu

src_root=${1:-}

if test -z "$src_root"; then
    printf '1..0 # SKIP src_root argument is required\n'
    exit 0
fi

echo "1..1"

if rg -n --fixed-strings "bluenoise_conf = g_sixel_bn_conf_8bit;" \
        "$src_root/src/dither-policy-a-dither.c" \
        "$src_root/src/dither-policy-x-dither.c" \
        "$src_root/src/dither-policy-bluenoise.c" >/dev/null 2>&1; then
    echo "not ok 1 - positional 8bit copies global bluenoise conf before init"
    exit 0
fi

if rg -n --fixed-strings "bluenoise_conf = g_sixel_bn_conf_float32;" \
        "$src_root/src/dither-policy-a-dither.c" \
        "$src_root/src/dither-policy-x-dither.c" \
        "$src_root/src/dither-policy-bluenoise.c" >/dev/null 2>&1; then
    echo "not ok 1 - positional float32 copies global bluenoise conf before init"
    exit 0
fi

echo "ok 1 - positional bluenoise config copy avoids unsynchronized globals"
exit 0
