#!/bin/sh
# Exercise the float32 Eytzinger lookup policy through img2sixel options.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${images_dir}/snake.png"
output_sixel="${output_dir}/eytzinger-float32.six"

require_file "${snake_png}"

if run_img2sixel --lookup-policy=eytzinger --precision=float32 -p 16 -d none \
        -o "${output_sixel}" "${snake_png}" 2>>"${log_file}"; then
    pass 1 "float32 Eytzinger lookup policy completes"
else
    fail 1 "float32 Eytzinger lookup policy failed"
fi

exit "${status}"
