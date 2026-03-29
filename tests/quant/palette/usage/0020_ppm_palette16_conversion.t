#!/bin/sh
# Convert PPM with a 16-colour palette and scaling.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm"
map16_png="${TOP_SRCDIR}/images/map16.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/ppm-palette16.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${map16_png}" -w100 -hauto -rbicubic -dauto "${snake_ppm}" >"${target_sixel}" || {
    echo "not ok" 1 - "PPM conversion with 16-colour palette fails"
    exit 0
}

echo "ok" 1 - "PPM conversion with 16-colour palette works"

exit 0
