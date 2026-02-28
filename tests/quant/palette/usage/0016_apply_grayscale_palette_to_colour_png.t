#!/bin/sh
# Apply grayscale palette file to a colour PNG input.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-palette-colour.sixel"

run_img2sixel -m "${snake_gray_png}" "${snake_png}" >"${target_sixel}" || {
    fail 1 "grayscale palette application fails"
    exit 0
}

pass 1 "grayscale palette applied to colour PNG"

exit 0
