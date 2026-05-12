#!/bin/sh
# TAP test covering bluenoise gradient_factor CLI override over env in float32.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_env=$(mktemp)
output_cli=$(mktemp)

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR=0 \
    -d bluenoise:scan=raster --precision=float32 -p 16 \
    -o "${output_env}" "${input_image}" || {
    echo "not ok" 1 - "float32 bluenoise env baseline failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR=0 \
    -d bluenoise:G1.0:scan=raster --precision=float32 -p 16 \
    -o "${output_cli}" "${input_image}" || {
    echo "not ok" 1 - "float32 bluenoise CLI gradient_factor override failed"
    exit 0
}

cmp -s "${output_env}" "${output_cli}" && {
    echo "not ok" 1 - "float32 bluenoise gradient_factor CLI override had no effect"
    exit 0
}

echo "ok" 1 - "float32 bluenoise gradient_factor CLI override takes precedence"
exit 0
