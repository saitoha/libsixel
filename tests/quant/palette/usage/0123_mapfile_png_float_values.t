#!/bin/sh
# Verify native float image-map export retains unmanaged PNG colors.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# The literal PLTE colors are sorted by the native Heckbert histogram.
# With eight distinct colors and no reduction, export must retain their
# original byte values, not the legacy histogram cell centers.
# CMS defaults are tested separately.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"
expected_palette='JASC-PAL
0100
8
0 0 0
3 171 185
3 175 3
123 102 248
149 157 3
159 3 3
190 3 180
191 191 191'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
        --precision-policy=float32 -m "${mapfile_png}" -M pal:- -o /dev/null \
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
    echo "not ok" 1 - "float PNG mapfile lost original color values"
    exit 0
}

echo "ok" 1 - "float PNG mapfile retains original color values"
exit 0
