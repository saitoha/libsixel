#!/bin/sh
# Verify img2sixel accepts a PAL1 PNG as a palette map.
# Policy: docs/encoder/palette-construction/external-palettes.md
#
# Test flow:
# - Use a PNGSuite palette (color type 3, 1-bit) image for -m.
# - Convert a standard input image with the palette mapfile.
# - Expect img2sixel to exit successfully.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

palette_png="${TOP_SRCDIR}/images/pngsuite/basic/basn3p01.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${palette_png}" "${input_png}" -o/dev/null || {
    echo "not ok" 1 - "PAL1 PNG palette mapfile rejected"
    exit 0
}

echo "ok" 1 - "PAL1 PNG palette mapfile accepted"

exit 0
