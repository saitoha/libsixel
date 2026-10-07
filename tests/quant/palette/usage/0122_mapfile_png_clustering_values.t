#!/bin/sh
# Verify PNG map clustering preserves the occupied sRGB8 fixture colors.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Preserve the unmanaged fixture values; CMS defaults are tested separately.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"
expected_palette='0 0 0
0100
123 102 248
149 157 3
159 3 3
190 3 180
191 191 191
3 171 185
3 175 3
8
JASC-PAL'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision=8bit -Wgamma -Fnone \
        -_none --binning-policy=none -X oklab -m "${mapfile_png}" -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "PNG mapfile export failed with -X oklab"
    exit 0
}
# Image-map indices/order are solver-owned; compare the occupied color set.
actual_palette=$(printf "%s\n" "${actual_palette}" | tr -d '\015' | LC_ALL=C sort) || {
    echo "not ok" 1 - "PNG mapfile output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "PNG mapfile lost occupied colors under -X oklab"
    exit 0
}

echo "ok" 1 - "PNG mapfile retains occupied colors after Oklab clustering"
exit 0
