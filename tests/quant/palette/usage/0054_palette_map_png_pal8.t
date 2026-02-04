#!/bin/sh
# Verify img2sixel accepts a PAL8 PNG as a palette map.
#
# Test flow:
# - Use a PNGSuite palette (color type 3, 8-bit) image for -m.
# - Convert a standard input image with the palette mapfile.
# - Expect img2sixel to exit successfully.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

palette_png="${images_dir}/pngsuite/basic/basn3p08.png"
input_png="${top_srcdir}/tests/data/inputs/snake_64.png"
target_sixel="${output_dir}/mapfile-pal8.sixel"

require_file "${palette_png}"
require_file "${input_png}"

if run_img2sixel -m "${palette_png}" "${input_png}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "PAL8 PNG palette mapfile accepted"
else
    fail 1 "PAL8 PNG palette mapfile rejected"
fi

exit "${status}"
