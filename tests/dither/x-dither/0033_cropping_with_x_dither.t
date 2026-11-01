#!/bin/sh
# Crop grayscale PNG using X ordered dither.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

status=0


snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/crop-x-dither.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -c200x200+100+100 -dx_dither "${snake_gray_png}" >"${target_sixel}" || {
    echo "not ok" 1 - "cropping with X ordered dither fails"
    exit "${status}"
}

echo "ok" 1 - "cropping with X ordered dither succeeds"
exit "${status}"
