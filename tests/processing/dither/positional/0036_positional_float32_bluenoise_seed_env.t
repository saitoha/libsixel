#!/bin/sh
# TAP test covering positional float32 bluenoise seed-based phase derivation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_DITHER_BLUENOISE_SEED=123 \
        -d bluenoise:scan=raster --precision=float32 -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "positional float32 bluenoise seed env failed"
    exit 0
}

echo "ok" 1 - "positional float32 bluenoise seed env passed"

exit 0
