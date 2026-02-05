#!/bin/sh
# Resize Sixel input while constraining palette size.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_six="${images_dir}/snake.six"
target_sixel="${output_dir}/sixel-resize.sixel"



if run_img2sixel -w200 -p8 "${snake_six}" \
        >"${target_sixel}"; then
    pass 1 "Sixel resizing with palette limit succeeds"
else
    fail 1 "Sixel resizing with palette limit fails"
fi

exit "${status}"
