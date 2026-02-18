#!/bin/sh
# Ensure 4-bit grayscale output succeeds.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_tga="${TOP_SRCDIR}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray4.sixel"

run_img2sixel -bgray4 -w120 "${snake_tga}" >"${target_sixel}" || {
    fail 1 "4-bit grayscale output fails"
    exit 0
}

pass 1 "4-bit grayscale output succeeds"

exit 0
