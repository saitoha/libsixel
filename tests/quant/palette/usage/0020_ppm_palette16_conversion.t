#!/bin/sh
# Convert PPM with a 16-colour palette and scaling.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ppm="${top_srcdir}/tests/data/inputs/snake_64.ppm"
map16_png="${images_dir}/map16.png"
target_sixel="${output_dir}/ppm-palette16.sixel"

require_file "${snake_ppm}"
require_file "${map16_png}"

if run_img2sixel -m "${map16_png}" -w100 -hauto -rbicubic -dauto \
        "${snake_ppm}" >"${target_sixel}"; then
    pass 1 "PPM conversion with 16-colour palette works"
else
    fail 1 "PPM conversion with 16-colour palette fails"
fi

exit "${status}"
