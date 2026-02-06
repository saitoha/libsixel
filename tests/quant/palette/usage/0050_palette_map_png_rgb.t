#!/bin/sh
# Verify img2sixel accepts an RGB PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite RGB (color type 2) image as the -m mapfile.
# - Convert a standard input image with the mapfile applied.
# - Expect img2sixel to exit successfully.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

palette_png="${images_dir}/pngsuite/basic/basn2c08.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-rgb.sixel"

if run_img2sixel -m "${palette_png}" "${input_png}" >"${target_sixel}"; then
    pass 1 "RGB PNG palette mapfile accepted"
else
    fail 1 "RGB PNG palette mapfile rejected"
fi

exit "${status}"
