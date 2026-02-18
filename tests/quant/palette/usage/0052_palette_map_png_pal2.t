#!/bin/sh
# Verify img2sixel accepts a PAL2 PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite palette (color type 3, 2-bit) image for -m.
# - Convert a standard input image with the palette mapfile.
# - Expect img2sixel to exit successfully.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

palette_png="${TOP_SRCDIR}/images/pngsuite/basic/basn3p02.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-pal2.sixel"

run_img2sixel -m "${palette_png}" "${input_png}" \
        >"${target_sixel}" || {
    fail 1 "PAL2 PNG palette mapfile rejected"
    exit 0
}

pass 1 "PAL2 PNG palette mapfile accepted"

exit 0
