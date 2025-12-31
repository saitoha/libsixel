#!/bin/sh
# Check inspection mode with diffusion and background colour.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

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
