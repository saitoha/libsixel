#!/bin/sh
# Apply external palette to grayscale PNG conversion.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
map8_palette="${TOP_SRCDIR}/images/map8-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png-palette.sixel"

run_img2sixel -m "${map8_palette}" "${snake_gray_png}" >"${target_sixel}" || {
    echo "not ok" 1 "grayscale PNG palette conversion fails"
    exit 0
}

echo "ok" 1 "grayscale PNG with external palette works"

exit 0
