#!/bin/sh
# Verify img2sixel accepts a G1 PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite grayscale (color type 0, 1-bit) image for -m.
# - Convert a standard input image with the palette mapfile.
# - Expect img2sixel to exit successfully.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

palette_png="${images_dir}/pngsuite/basic/basn0g01.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-g1.sixel"

if run_img2sixel -m "${palette_png}" "${input_png}" \
        >"${target_sixel}"; then
    pass 1 "G1 PNG palette mapfile accepted"
else
    fail 1 "G1 PNG palette mapfile rejected"
fi

exit "${status}"
