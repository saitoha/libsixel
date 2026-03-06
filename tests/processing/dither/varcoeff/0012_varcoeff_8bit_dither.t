#!/bin/sh
# TAP test ensuring LSO2 dither exercises the 8-bit variable-coefficient path.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/varcoeff-8bit.six"

run_img2sixel -d lso2 -y raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${snake_png}" || {
    echo "not ok" 1 - "variable-coefficient 8-bit dither failed"
    exit 0
}

echo "ok" 1 - "variable-coefficient 8-bit dither completes"

exit 0
