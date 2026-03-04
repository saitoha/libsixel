#!/bin/sh
# TAP test: coregraphics decodes grayscale HEIF content with stable quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heif-gray-lossless-64.heif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_heif_gray_lossless.six" || {
    echo "not ok" 1 "coregraphics grayscale HEIF decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.ppm" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_heif_gray_lossless.six" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "coregraphics grayscale HEIF decode preserves quality"
exit 0
