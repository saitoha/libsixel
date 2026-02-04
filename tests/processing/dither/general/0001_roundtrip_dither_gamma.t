#!/bin/sh
# Verify round-trip conversion with dithering and gamma correction.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_jpg="${images_dir}/snake.jpg"
snake_roundtrip="${tmp_dir}/snake-roundtrip.sixel"

require_file "${snake_jpg}"

if run_img2sixel "${snake_jpg}" -datkinson -flum -save \
    | run_img2sixel | tee "${snake_roundtrip}" >/dev/null 2>>"${log_file}"; then
    pass 1 "round-trip conversion with dithering and gamma succeeded"
else
    fail 1 "round-trip conversion failed"
fi

exit "${status}"
