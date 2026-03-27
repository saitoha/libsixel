#!/bin/sh
# Apply grayscale palette file to a colour PNG input.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-palette-colour.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${snake_gray_png}" "${snake_png}" >"${target_sixel}" || {
    echo "not ok" 1 - "grayscale palette application fails"
    exit 0
}

echo "ok" 1 - "grayscale palette applied to colour PNG"

exit 0
