#!/bin/sh
# TAP test ensuring positional dithering exercises the 8-bit pipeline.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/positional-8bit.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d a_dither -y raster --precision=8bit -p 16 \
        -o "${output_sixel}" "${snake_png}" || {
    echo "not ok" 1 - "positional 8-bit dither failed"
    exit 0
}

echo "ok" 1 - "positional 8-bit dither completes"

exit 0
