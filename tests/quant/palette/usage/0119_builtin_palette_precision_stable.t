#!/bin/sh
# Verify precision mode does not reinterpret built-in palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
expected_palette='JASC-PAL
0100
16
0 0 0
128 0 0
0 128 0
128 128 0
0 0 128
128 0 128
0 128 128
192 192 192
128 128 128
255 0 0
0 255 0
255 255 0
0 0 255
255 0 255
0 255 255
255 255 255'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --precision=float32 -bxterm16 -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette export failed with float32 precision"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "builtin palette changed under float32 precision"
    exit 0
}

echo "ok" 1 - "builtin palette is stable under precision mode"
exit 0
