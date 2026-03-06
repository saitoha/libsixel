#!/bin/sh
# Crop grayscale PNG using alternate ordered dither.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/crop-alt-dither.sixel"

run_img2sixel -c200x200+100+100 -w400 -da_dither \
        "${snake_gray_png}" >"${target_sixel}" || {
    echo "not ok" 1 - "cropping with alternate dither fails"
    exit 0
}

echo "ok" 1 - "cropping with alternate dither succeeds"

exit 0
