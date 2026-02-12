#!/bin/sh
# Verify img2sixel accepts a PAL4 PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite palette (color type 3, 4-bit) image for -m.
# - Convert a standard input image with the palette mapfile.
# - Expect img2sixel to exit successfully.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

palette_png="${images_dir}/pngsuite/basic/basn3p04.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-pal4.sixel"

run_img2sixel -m "${palette_png}" "${input_png}" >"${target_sixel}" || {
    fail 1 "PAL4 PNG palette mapfile rejected"
    exit 0
}

pass 1 "PAL4 PNG palette mapfile accepted"

exit 0
