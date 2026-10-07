#!/bin/sh
# Verify a byte G2 image map reconstructs its four decoded gray values.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# The fixture uses every unsigned 2-bit sample. PNG expansion is s * 255 / 3,
# giving exact bytes 0, 85, 170 and 255 on all three axes. With CMS and the
# precision-requiring policies disabled, gamma-byte reconstruction retains
# these four occupied colors. Palette order is not an image-map contract.
expected_palette='0 0 0
0100
170 170 170
255 255 255
4
85 85 85
JASC-PAL'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=none! \
        --precision=8bit -Xgamma -Wgamma -0hard -Fnone -_none \
        -~none -dnone -m "${TOP_SRCDIR}/images/pngsuite/basic/basn0g02.png" \
        -M pal-jasc:- -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
) || {
    echo "not ok" 1 - "G2 PNG palette export failed"
    exit 0
}
actual_palette=$(printf "%s\n" "${actual_palette}" | tr -d '\015' | LC_ALL=C sort) || {
    echo "not ok" 1 - "G2 PNG palette normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "G2 PNG map did not retain its decoded gray values"
    exit 0
}

echo "ok" 1 - "G2 PNG map retains its four decoded gray values"

exit 0
