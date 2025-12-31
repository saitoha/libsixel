#!/bin/sh
# Ensure 8-bit grayscale output succeeds.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_tga="${images_dir}/snake.tga"
target_sixel="${output_dir}/gray8.sixel"

require_file "${snake_tga}"

if run_img2sixel -bgray8 -w120 "${snake_tga}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "8-bit grayscale output succeeds"
else
    fail 1 "8-bit grayscale output fails"
fi

exit "${status}"
