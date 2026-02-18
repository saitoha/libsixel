#!/bin/sh
# TAP test covering positional float32 x_dither with invalid strength env.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel --env SIXEL_DITHER_X_DITHER_STRENGTH=abc \
        -d x_dither -y raster --precision=float32 -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional float32 x_dither invalid strength env failed"
    exit 0
}

pass 1 "positional float32 x_dither invalid strength env passed"

exit 0
