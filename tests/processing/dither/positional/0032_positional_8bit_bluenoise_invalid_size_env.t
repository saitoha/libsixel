#!/bin/sh
# TAP test covering positional 8-bit bluenoise invalid size env fallback.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel --env SIXEL_DITHER_BLUENOISE_SIZE=63 \
        -d bluenoise -y raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    fail 1 "positional 8-bit bluenoise invalid size env failed"
    exit 0
}

pass 1 "positional 8-bit bluenoise invalid size env passed"

exit 0
