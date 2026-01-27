#!/bin/sh
# Apply external palette to grayscale PNG conversion.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
map8_palette="${images_dir}/map8-palette.png"
target_sixel="${output_dir}/gray-png-palette.sixel"

require_file "${snake_gray_png}"
require_file "${map8_palette}"

if run_img2sixel -m "${map8_palette}" "${snake_gray_png}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "grayscale PNG with external palette works"
else
    fail 1 "grayscale PNG palette conversion fails"
fi

exit "${status}"
