#!/bin/sh
# TAP test covering positional 8-bit x_dither with serpentine scan.

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

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d x_dither -y serpentine --precision=8bit -p 16 \
        -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "positional 8-bit x_dither serpentine failed"
    exit 0
}

echo "ok" 1 - "positional 8-bit x_dither serpentine passed"

exit 0
