#!/bin/sh
# TAP test covering positional 8-bit a_dither with forced palette size.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d a_dither -y raster --precision=8bit -p 16! \
        -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "positional 8-bit a_dither forced palette failed"
    exit 0
}

echo "ok" 1 - "positional 8-bit a_dither forced palette passed"

exit 0
