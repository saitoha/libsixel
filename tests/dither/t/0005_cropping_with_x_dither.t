#!/bin/sh
# Crop grayscale PNG using X ordered dither.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${output_dir}/crop-x-dither.sixel"

require_file "${snake_gray_png}"

if run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "cropping with X ordered dither succeeds"
else
    fail 1 "cropping with X ordered dither fails"
fi

exit "${status}"
