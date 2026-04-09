#!/bin/sh
# TAP test covering temporal-diffusion acceptance on the palette path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d temporal-diffusion -y serpentine -p 16 \
    -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "temporal-diffusion with scan option failed"
    exit 0
}

test -s "${output_sixel}" || {
    echo "not ok" 1 - "temporal-diffusion output is empty"
    exit 0
}

echo "ok" 1 - "temporal-diffusion accepts -y scan option"
exit 0
