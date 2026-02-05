#!/bin/sh
# Exercise the 8-bit Eytzinger lookup policy through img2sixel options.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${output_dir}/eytzinger-8bit.six"

require_file "${snake_png}"

if run_img2sixel --lookup-policy=eytzinger -p 16 -d none \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "8-bit Eytzinger lookup policy completes"
else
    fail 1 "8-bit Eytzinger lookup policy failed"
fi

exit "${status}"
