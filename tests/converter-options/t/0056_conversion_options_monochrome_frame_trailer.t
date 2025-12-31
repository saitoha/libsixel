#!/bin/sh
# Emit monochrome frame and append trailer marker.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
target_sixel="${output_dir}/monochrome-frame.sixel"
trailer_txt="${output_dir}/monochrome-frame-trailer.txt"

require_file "${snake_jpg}"

if run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    printf '\033[*1z' >"${trailer_txt}"
    pass 1 "monochrome frame with trailer succeeds"
else
    fail 1 "monochrome frame with trailer fails"
fi

exit "${status}"
