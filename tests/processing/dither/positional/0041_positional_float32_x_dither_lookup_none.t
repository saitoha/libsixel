#!/bin/sh
# TAP test covering positional float32 x_dither with lookup-policy none.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

if run_img2sixel -d x_dither -y raster --precision=float32 \
        --lookup-policy=none -p 16 -o "${output_sixel}" "${input_image}"; then
    pass 1 "positional float32 x_dither lookup none passed"
else
    fail 1 "positional float32 x_dither lookup none failed"
fi

exit 0
