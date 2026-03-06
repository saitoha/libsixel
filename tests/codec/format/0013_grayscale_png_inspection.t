#!/bin/sh
# Inspect grayscale PNG without errors.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
target_txt="${ARTIFACT_LOCAL_DIR}/gray-png-inspection.txt"

run_img2sixel -I "${snake_gray_png}" >"${target_txt}" || {
    echo "not ok" 1 - "grayscale PNG inspection fails"
    exit 0
}

echo "ok" 1 - "grayscale PNG inspection succeeds"

exit 0
