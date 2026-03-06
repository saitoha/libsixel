#!/bin/sh
# Check inspection mode with diffusion and background colour.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

snake_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

run_img2sixel -I -dstucki -thls -B"#a0B030" "${snake_ppm}" >"${target_txt}" || {
    echo "not ok" 1 - "inspection with diffusion failed"
    exit 0
}

echo "ok" 1 - "inspection with diffusion and background works"
exit 0
