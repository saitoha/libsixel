#!/bin/sh
# Exercise the float32 VP-tree lookup policy through img2sixel options.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${images_dir}/snake.png"
output_sixel="${output_dir}/vptree-float32.six"

require_file "${snake_png}"

if run_img2sixel --lookup-policy=vptree --precision=float32 -p 16 -d none \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "float32 VP-tree lookup policy completes"
else
    fail 1 "float32 VP-tree lookup policy failed"
fi

exit "${status}"
