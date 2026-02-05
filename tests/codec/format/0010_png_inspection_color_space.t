#!/bin/sh
# Verify PNG inspection sets colour space in the report.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${images_dir}/snake.png"
target_txt="${output_dir}/png-inspection.txt"



if run_img2sixel -I -C10 -djajuni "${snake_png}" \
        >"${target_txt}"; then
    pass 1 "PNG inspection sets colour space"
else
    fail 1 "PNG inspection colour space failed"
fi

exit "${status}"
