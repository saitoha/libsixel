#!/bin/sh
# Verify img2sixel accepts an RGB PNG as a palette map.
# Policy: docs/encoder/palette-construction/external-palettes.md
#
# Test flow:
# - Use a PNGSuite RGB (color type 2) image as the -m mapfile.
# - Convert a standard input image with the mapfile applied.
# - Expect img2sixel to exit successfully.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

palette_png="${TOP_SRCDIR}/images/pngsuite/basic/basn2c08.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${palette_png}" "${input_png}" -o/dev/null || {
    echo "not ok" 1 - "RGB PNG palette mapfile rejected"
    exit 0
}

echo "ok" 1 - "RGB PNG palette mapfile accepted"

exit 0
