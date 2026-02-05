#!/bin/sh
# Inspect grayscale JPEG without errors.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_jpg="${images_dir}/snake-grayscale.jpg"
target_txt="${output_dir}/gray-jpeg-inspection.txt"



if run_img2sixel -I "${snake_gray_jpg}" >"${target_txt}"; then
    pass 1 "grayscale JPEG inspection succeeds"
else
    fail 1 "grayscale JPEG inspection fails"
fi

exit "${status}"
