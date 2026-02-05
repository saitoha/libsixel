#!/bin/sh
# Verify img2sixel accepts an RGB PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite RGB (color type 2) image as the -m mapfile.
# - Convert a standard input image with the mapfile applied.
# - Expect img2sixel to exit successfully.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

palette_png="${images_dir}/pngsuite/basic/basn2c08.png"
input_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/mapfile-rgb.sixel"

if run_img2sixel -m "${palette_png}" "${input_png}" \
        >"${target_sixel}"; then
    pass 1 "RGB PNG palette mapfile accepted"
else
    fail 1 "RGB PNG palette mapfile rejected"
fi

exit "${status}"
