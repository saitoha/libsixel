#!/bin/sh
# Convert PPM with a 16-colour palette and scaling.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_ppm="${images_dir}/snake.ppm"
map16_png="${images_dir}/map16.png"
target_sixel="${output_dir}/ppm-palette16.sixel"

require_file "${snake_ppm}"
require_file "${map16_png}"

if run_img2sixel -m "${map16_png}" -w100 -hauto -rbicubic -dauto \
        "${snake_ppm}" >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "PPM conversion with 16-colour palette works"
else
    fail 1 "PPM conversion with 16-colour palette fails"
fi

exit "${status}"
