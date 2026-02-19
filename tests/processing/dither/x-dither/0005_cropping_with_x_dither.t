#!/bin/sh
# Crop grayscale PNG using X ordered dither.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/crop-x-dither.sixel"

run_img2sixel -c200x200+100+100 -dx_dither "${snake_gray_png}" >"${target_sixel}" || {
    fail 1 "cropping with X ordered dither fails"
    exit "${status}"
}

pass 1 "cropping with X ordered dither succeeds"
exit "${status}"
