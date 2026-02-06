#!/bin/sh
# TAP test ensuring positional dithering exercises the 8-bit pipeline.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

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
