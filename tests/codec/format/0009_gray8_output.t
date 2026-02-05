#!/bin/sh
# Ensure 8-bit grayscale output succeeds.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_tga="${images_dir}/snake.tga"
target_sixel="${output_dir}/gray8.sixel"



if run_img2sixel -bgray8 -w120 "${snake_tga}" \
        >"${target_sixel}"; then
    pass 1 "8-bit grayscale output succeeds"
else
    fail 1 "8-bit grayscale output fails"
fi

exit "${status}"
