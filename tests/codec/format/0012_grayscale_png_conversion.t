#!/bin/sh
# Convert grayscale PNG without palette overrides.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_gray_png="${TOP_SRCDIR}/images/snake-grayscale.png"
target_sixel="${ARTIFACT_LOCAL_DIR}/gray-png.sixel"

run_img2sixel "${snake_gray_png}" >"${target_sixel}" || {
    fail 1 "grayscale PNG conversion fails"
    exit 0
}

pass 1 "grayscale PNG conversion succeeds"

exit 0
