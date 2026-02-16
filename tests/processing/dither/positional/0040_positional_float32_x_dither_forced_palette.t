#!/bin/sh
# TAP test covering positional float32 x_dither with forced palette size.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel -d x_dither -y raster --precision=float32 -p 16! \
        -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional float32 x_dither forced palette failed"
    exit 0
}

pass 1 "positional float32 x_dither forced palette passed"

exit 0
