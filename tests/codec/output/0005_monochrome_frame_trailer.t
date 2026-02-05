#!/bin/sh
# Emit monochrome frame and append trailer marker.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
target_sixel="${output_dir}/monochrome-frame.sixel"
trailer_txt="${output_dir}/monochrome-frame-trailer.txt"



if run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" \
        >"${target_sixel}"; then
    printf '\033[*1z' >"${trailer_txt}"
    pass 1 "monochrome frame with trailer succeeds"
else
    fail 1 "monochrome frame with trailer fails"
fi

exit "${status}"
