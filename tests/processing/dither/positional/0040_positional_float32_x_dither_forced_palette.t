#!/bin/sh
# TAP test covering positional float32 x_dither with forced palette size.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel -d x_dither -y raster --precision=float32 -p 16! \
        -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "positional float32 x_dither forced palette failed"
    exit 0
}

echo "ok" 1 - "positional float32 x_dither forced palette passed"

exit 0
