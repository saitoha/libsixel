#!/bin/sh
# TAP test ensuring positional dithering exercises the 8-bit pipeline.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/positional-8bit.six"

if run_img2sixel -d a_dither -y raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${snake_png}"; then
    pass 1 "positional 8-bit dither completes"
else
    fail 1 "positional 8-bit dither failed"
fi

exit "${status}"
