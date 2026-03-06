#!/bin/sh
# Convert PPM with a 16-colour palette and scaling.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm"
map16_png="${TOP_SRCDIR}/images/map16.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/ppm-palette16.sixel"

run_img2sixel -m "${map16_png}" -w100 -hauto -rbicubic -dauto "${snake_ppm}" >"${target_sixel}" || {
    echo "not ok" 1 - "PPM conversion with 16-colour palette fails"
    exit 0
}

echo "ok" 1 - "PPM conversion with 16-colour palette works"

exit 0
