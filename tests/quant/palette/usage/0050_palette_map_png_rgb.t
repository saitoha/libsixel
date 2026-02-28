#!/bin/sh
# Verify img2sixel accepts an RGB PNG as a palette map.
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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

palette_png="${TOP_SRCDIR}/images/pngsuite/basic/basn2c08.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-rgb.sixel"

run_img2sixel -m "${palette_png}" "${input_png}" >"${target_sixel}" || {
    echo "not ok" 1 "RGB PNG palette mapfile rejected"
    exit 0
}

echo "ok" 1 "RGB PNG palette mapfile accepted"

exit 0
