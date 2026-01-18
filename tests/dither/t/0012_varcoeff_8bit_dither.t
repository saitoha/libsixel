#!/bin/sh
# TAP test ensuring LSO2 dither exercises the 8-bit variable-coefficient path.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_png="${images_dir}/snake.png"
output_sixel="${output_dir}/varcoeff-8bit.six"

require_file "${snake_png}"

if run_img2sixel -d lso2 -y raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${snake_png}" 2>>"${log_file}"; then
    pass 1 "variable-coefficient 8-bit dither completes"
else
    fail 1 "variable-coefficient 8-bit dither failed"
fi

exit "${status}"
