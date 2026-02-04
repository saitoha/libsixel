#!/bin/sh
# Ensure inspection mode accepts high color conversion with Sierra-2.
#
# Steps:
# - Read a standard RGB test image.
# - Run img2sixel with inspection (-I) and -d sierra2.
# - Only confirm that the command exits successfully.

set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${images_dir}/snake.png"
require_file "${input_image}"

target_txt="${output_dir}/inspection.txt"

if run_img2sixel -I -d sierra2 "${input_image}" \
        >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "inspection with high color and Sierra-2 exits cleanly"
else
    fail 1 "inspection with high color and Sierra-2 failed"
fi

exit "${status}"
