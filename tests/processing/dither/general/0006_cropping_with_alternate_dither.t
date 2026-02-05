#!/bin/sh
# Crop grayscale PNG using alternate ordered dither.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${output_dir}/crop-alt-dither.sixel"

require_file "${snake_gray_png}"

if run_img2sixel -c200x200+100+100 -w400 -da_dither \
        "${snake_gray_png}" >"${target_sixel}"; then
    pass 1 "cropping with alternate dither succeeds"
else
    fail 1 "cropping with alternate dither fails"
fi

exit "${status}"
