#!/bin/sh
# Apply grayscale palette file to a colour PNG input.
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
snake_png="${images_dir}/snake.png"
target_sixel="${output_dir}/gray-palette-colour.sixel"

require_file "${snake_gray_png}"
require_file "${snake_png}"

if run_img2sixel -m "${snake_gray_png}" "${snake_png}" \
        >"${target_sixel}" 2>>"${log_file}"; then
    pass 1 "grayscale palette applied to colour PNG"
else
    fail 1 "grayscale palette application fails"
fi

exit "${status}"
