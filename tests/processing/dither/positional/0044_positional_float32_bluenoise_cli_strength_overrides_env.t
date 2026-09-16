#!/bin/sh
# TAP test covering bluenoise strength CLI override over environment in float32.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_env="${ARTIFACT_LOCAL_DIR}/output_env.six"
output_cli="${ARTIFACT_LOCAL_DIR}/output_cli.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.00 \
    -d bluenoise:scan=raster --precision=float32 -p 16 \
    -o "${output_env}" "${input_image}" || {
    echo "not ok" 1 - "float32 bluenoise env baseline failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_DITHER_BLUENOISE_STRENGTH=0.00 \
    -d bluenoise:strength=0.20:scan=raster --precision=float32 -p 16 \
    -o "${output_cli}" "${input_image}" || {
    echo "not ok" 1 - "float32 bluenoise strength override failed"
    exit 0
}

cmp -s "${output_env}" "${output_cli}" && {
    echo "not ok" 1 - "float32 bluenoise strength CLI override had no effect"
    exit 0
}

echo "ok" 1 - "float32 bluenoise strength CLI override takes precedence"
exit 0
