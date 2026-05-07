#!/bin/sh
# Verify precision mode does not reinterpret PNG mapfile palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"
expected_palette='JASC-PAL
0100
8
162 6 6
6 178 6
150 158 6
126 106 250
194 6 182
6 174 186
194 194 194
2 2 2'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --precision=float32 -m "${mapfile_png}" -M pal:- -o /dev/null \
        "${input_image}"
) || {
    echo "not ok" 1 - "PNG mapfile export failed with float32 precision"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "PNG mapfile output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "PNG mapfile changed under float32 precision"
    exit 0
}

echo "ok" 1 - "PNG mapfile is stable under precision mode"
exit 0
