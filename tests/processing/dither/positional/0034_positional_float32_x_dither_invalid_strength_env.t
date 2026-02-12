#!/bin/sh
# TAP test covering positional float32 x_dither with invalid strength env.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel --env SIXEL_DITHER_X_DITHER_STRENGTH=abc \
        -d x_dither -y raster --precision=float32 -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional float32 x_dither invalid strength env failed"
    exit 0
}

pass 1 "positional float32 x_dither invalid strength env passed"

exit 0
