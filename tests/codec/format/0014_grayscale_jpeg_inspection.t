#!/bin/sh
# Inspect grayscale JPEG without errors.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_gray_jpg="${TOP_SRCDIR}/images/snake-grayscale.jpg"
target_txt="${ARTIFACT_LOCAL_DIR}/gray-jpeg-inspection.txt"

run_img2sixel -I "${snake_gray_jpg}" >"${target_txt}" || {
    echo "not ok" 1 - "grayscale JPEG inspection fails"
    exit 0
}

echo "ok" 1 - "grayscale JPEG inspection succeeds"

exit 0
