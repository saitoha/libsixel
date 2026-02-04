#!/bin/sh
# Check inspection mode with diffusion and background colour.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"
require_file "${snake_ppm}"

target_txt="${output_dir}/inspection.txt"

if run_img2sixel -I -dstucki -thls -B"#a0B030" "${snake_ppm}" \
        >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "inspection with diffusion and background works"
else
    fail 1 "inspection with diffusion failed"
fi

exit "${status}"
