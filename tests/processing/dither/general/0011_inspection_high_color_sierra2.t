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

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

if run_img2sixel -I -d sierra2 "${input_image}" >"${target_txt}"; then
    pass 1 "inspection with high color and Sierra-2 exits cleanly"
else
    fail 1 "inspection with high color and Sierra-2 failed"
fi

exit "${status}"
