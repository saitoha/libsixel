#!/bin/sh
# TAP test covering positional 8-bit bluenoise RGB channel mode env value.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
case_id=${test_name%.t}
output_sixel="${ARTIFACT_LOCAL_DIR}/${case_id}.six"

if run_img2sixel --env SIXEL_DITHER_BLUENOISE_CHANNEL=rgb \
        -d bluenoise -y raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${input_image}"; then
    pass 1 "positional 8-bit bluenoise rgb channel env passed"
else
    fail 1 "positional 8-bit bluenoise rgb channel env failed"
fi

exit 0
