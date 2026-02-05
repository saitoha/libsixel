#!/bin/sh
# Convert grayscale PNG without palette overrides.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_gray_png="${images_dir}/snake-grayscale.png"
target_sixel="${output_dir}/gray-png.sixel"

require_file "${snake_gray_png}"

if run_img2sixel "${snake_gray_png}" >"${target_sixel}"; then
    pass 1 "grayscale PNG conversion succeeds"
else
    fail 1 "grayscale PNG conversion fails"
fi

exit "${status}"
