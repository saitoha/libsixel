#!/bin/sh
# TAP test ensuring positional dithering exercises the float32 pipeline.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/positional-float32.six"

run_img2sixel -d x_dither -y serpentine --precision=float32 -p 16 -o "${output_sixel}" "${snake_png}" || {
    echo "not ok" 1 - "positional float32 dither failed"
    exit "${status}"
}

echo "ok" 1 - "positional float32 dither completes"
exit "${status}"
