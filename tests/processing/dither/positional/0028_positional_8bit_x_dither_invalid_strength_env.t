#!/bin/sh
# TAP test covering positional 8-bit x_dither with invalid strength env value.

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_DITHER_X_DITHER_STRENGTH=1.2.3 \
        -d x_dither:scan=raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "positional 8-bit x_dither invalid strength env failed"
    exit 0
}

echo "ok" 1 - "positional 8-bit x_dither invalid strength env passed"

exit 0
