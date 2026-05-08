#!/bin/sh
# Verify working/output colorspaces do not reinterpret built-in palettes.
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
        --working-colorspace=linear --output-colorspace=linear \
        -bxterm16 -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette export failed with -W/-U linear"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "builtin palette output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "builtin palette changed under -W/-U linear"
    exit 0
}

echo "ok" 1 - "built-in palette is stable under working/output colorspaces"
exit 0
