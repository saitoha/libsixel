#!/bin/sh
# Inspect grayscale PNG without errors.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_txt="${output_dir}/gray-png-inspection.txt"

require_file "${snake_gray_png}"

if run_img2sixel -I "${snake_gray_png}" >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "grayscale PNG inspection succeeds"
else
    fail 1 "grayscale PNG inspection fails"
fi

exit "${status}"
