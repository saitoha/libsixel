#!/bin/sh
# TAP test ensuring positional dithering exercises the float32 pipeline.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/positional-float32.six"

run_img2sixel -d x_dither -y serpentine --precision=float32 -p 16 -o "${output_sixel}" "${snake_png}" || {
    fail 1 "positional float32 dither failed"
    exit "${status}"
}

pass 1 "positional float32 dither completes"
exit "${status}"
