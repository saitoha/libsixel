#!/bin/sh
# Verify an indexed PNG mapfile omits a PLTE entry unused by its pixels.
# Policy: docs/encoder/palette-construction/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Preserve the unmanaged fixture values; CMS defaults are tested separately.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"
expected_palette='JASC-PAL
0100
8
159 3 3
3 175 3
149 157 3
123 102 248
190 3 180
3 171 185
191 191 191
0 0 0'

# The fixture has PLTE entries 0 through 8, but its pixels use only 0 through 7.
actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none -m "${mapfile_png}" \
        -M pal-jasc:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "indexed PNG mapfile export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "indexed PNG palette normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "unused indexed PNG PLTE entry was retained"
    exit 0
}

echo "ok" 1 - "indexed PNG mapfiles omit unused PLTE entries"
exit 0
