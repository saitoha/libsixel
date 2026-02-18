#!/bin/sh
# Inspect grayscale JPEG without errors.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_gray_jpg="${TOP_SRCDIR}/images/snake-grayscale.jpg"
target_txt="${ARTIFACT_LOCAL_DIR}/gray-jpeg-inspection.txt"

run_img2sixel -I "${snake_gray_jpg}" >"${target_txt}" || {
    fail 1 "grayscale JPEG inspection fails"
    exit 0
}

pass 1 "grayscale JPEG inspection succeeds"

exit 0
