#!/bin/sh
# Convert grayscale PNG without palette overrides.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${output_dir}/gray-png.sixel"

require_file "${snake_gray_png}"

if run_img2sixel "${snake_gray_png}" >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "grayscale PNG conversion succeeds"
else
    fail 1 "grayscale PNG conversion fails"
fi

exit "${status}"
