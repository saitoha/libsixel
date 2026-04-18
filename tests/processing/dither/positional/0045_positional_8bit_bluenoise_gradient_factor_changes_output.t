#!/bin/sh
# TAP test covering 8bit bluenoise gradient_factor output difference.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_zero=$(mktemp)
output_grad=$(mktemp)

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR=0 \
    -d bluenoise:scan=raster -p 16 \
    -o "${output_zero}" "${input_image}" || {
    echo "not ok" 1 - "8bit bluenoise zero gradient_factor baseline failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR=1 \
    -d bluenoise:scan=raster -p 16 \
    -o "${output_grad}" "${input_image}" || {
    echo "not ok" 1 - "8bit bluenoise gradient_factor run failed"
    exit 0
}

cmp -s "${output_zero}" "${output_grad}" && {
    echo "not ok" 1 - "8bit bluenoise gradient_factor produced no output change"
    exit 0
}

echo "ok" 1 - "8bit bluenoise gradient_factor changes output"
exit 0
