#!/bin/sh
# Verify precision mode does not reinterpret monochrome palettes.
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
2
0 0 0
255 255 255'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --precision=float32 -e -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "monochrome palette export failed with float32 precision"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "monochrome palette output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "monochrome palette changed under float32 precision"
    exit 0
}

echo "ok" 1 - "monochrome palette is stable under precision mode"
exit 0
