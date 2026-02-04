#!/bin/sh
# Convert JPEG using external palette and Welsh filter.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

egret_jpg="${images_dir}/egret.jpg"
map8_png="${images_dir}/map8.png"
target_sixel="${output_dir}/jpeg-welsh.sixel"

require_file "${egret_jpg}"
require_file "${map8_png}"

if run_img2sixel -m "${map8_png}" -w200 -fau -rwelsh "${egret_jpg}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "JPEG conversion using palette and Welsh filter"
else
    fail 1 "JPEG palette Welsh conversion fails"
fi

exit "${status}"
