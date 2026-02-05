#!/bin/sh
# Ensure inspection mode accepts high color conversion with RGBA input.
#
# Steps:
# - Read a PNG image that includes an alpha channel.
# - Run img2sixel with inspection (-I).
# - Only confirm that the command exits successfully.

set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

input_image="${images_dir}/pngsuite/basic/basn6a08.png"

echo "1..1"
set -v

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

if run_img2sixel -I "${input_image}" >"${target_txt}"; then
    pass 1 "inspection with high color and RGBA input exits cleanly"
else
    fail 1 "inspection with high color and RGBA input failed"
fi

exit "${status}"
