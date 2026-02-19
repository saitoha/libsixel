#!/bin/sh
# TAP test covering positional float32 x_dither with lookup-policy none.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel -d x_dither -y raster --precision=float32 \
        --lookup-policy=none -p 16 -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional float32 x_dither lookup none failed"
    exit 0
}

pass 1 "positional float32 x_dither lookup none passed"

exit 0
