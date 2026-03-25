#!/bin/sh
# Verify libwebp animation path ignores invalid embedded ICC when cms=1.

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

input_webp_noicc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_noicc_anim2.webp"
input_webp_invalid_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_invalid_icc_anim2.webp"
output_noicc="${ARTIFACT_LOCAL_DIR}/webp-anim-noicc-cms1-invalid.sixel"
output_invalid_icc="${ARTIFACT_LOCAL_DIR}/webp-anim-invalid-icc-cms1.sixel"

run_img2sixel -Llibwebp:cms_engine=auto! -S "${input_webp_noicc}" >"${output_noicc}" || {
    echo "not ok" 1 - "libwebp animation decode failed for non-ICC input (cms=1)"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=auto! -S "${input_webp_invalid_icc}" >"${output_invalid_icc}" || {
    echo "not ok" 1 - "libwebp animation decode failed for invalid ICC input (cms=1)"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_noicc}" "${output_invalid_icc}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "animation cms=1 keeps output unchanged for invalid ICC chunk"
exit 0
