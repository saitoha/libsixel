#!/bin/sh
# Verify libwebp cms=1 decodes embedded ICC differently from non-ICC input.

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
output_noicc="${ARTIFACT_LOCAL_DIR}/webp-noicc.sixel"
output_icc="${ARTIFACT_LOCAL_DIR}/webp-icc.sixel"

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp_noicc}" >"${output_noicc}" || {
    echo "not ok" 1 - "libwebp decode failed for non-ICC input"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp_icc}" >"${output_icc}" || {
    echo "not ok" 1 - "libwebp decode failed for ICC input"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -b "MS-SSIM:0.999" "${output_noicc}" "${output_icc}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "cms=1 changes libwebp output when ICC chunk is present"
exit 0
