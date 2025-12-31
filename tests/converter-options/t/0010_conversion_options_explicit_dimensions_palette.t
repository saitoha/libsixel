#!/bin/sh
# Check explicit dimensions and palette options work together.
set -eux

. "$(CDPATH=; cd "$(dirname "$0")" && pwd)/converter_options_common.sh"

test_name=$(basename "$0")
setup_converter_options_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_jpg="${images_dir}/snake.jpg"
snake_dims="${tmp_dir}/snake-dims.sixel"

require_file "${snake_jpg}"

if run_img2sixel -w210 -h210 -djajuni -bxterm256 -o "${snake_dims}" \
    <"${snake_jpg}" 2>>"${log_file}"; then
    pass 1 "explicit dimensions and palette options succeeded"
else
    fail 1 "explicit dimensions and palette options failed"
fi

exit "${status}"
