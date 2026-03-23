#!/bin/sh
# Verify libwebp cms=0 ignores embedded ICC profile content.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp_noicc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_noicc.webp"
input_webp_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
output_noicc="${ARTIFACT_LOCAL_DIR}/webp-noicc-cms0.sixel"
output_icc="${ARTIFACT_LOCAL_DIR}/webp-icc-cms0.sixel"

run_img2sixel -Llibwebp:cms=0! "${input_webp_noicc}" >"${output_noicc}" || {
    echo "not ok" 1 - "libwebp decode failed for non-ICC input (cms=0)"
    exit 0
}

run_img2sixel -Llibwebp:cms=0! "${input_webp_icc}" >"${output_icc}" || {
    echo "not ok" 1 - "libwebp decode failed for ICC input (cms=0)"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_noicc}" "${output_icc}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "cms=0 keeps libwebp output unchanged for ICC chunk"
exit 0
