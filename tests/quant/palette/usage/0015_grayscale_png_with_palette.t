#!/bin/sh
# Apply external palette to grayscale PNG conversion.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
map8_palette="${TOP_SRCDIR}/images/map8-palette.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png-palette.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${map8_palette}" "${snake_gray_png}" >"${target_sixel}" || {
    echo "not ok" 1 - "grayscale PNG palette conversion fails"
    exit 0
}

echo "ok" 1 - "grayscale PNG with external palette works"

exit 0
