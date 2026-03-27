#!/bin/sh
# Verify libwebp cms=1 ignores invalid embedded ICC profile payloads.

set -eux

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp_noicc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_noicc.webp"
input_webp_invalid_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_invalid_icc.webp"
output_noicc="${ARTIFACT_LOCAL_DIR}/webp-noicc-cms1-invalid.sixel"
output_invalid_icc="${ARTIFACT_LOCAL_DIR}/webp-invalid-icc-cms1.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp_noicc}" >"${output_noicc}" || {
    echo "not ok" 1 - "libwebp decode failed for non-ICC input (cms=1)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! "${input_webp_invalid_icc}" >"${output_invalid_icc}" || {
    echo "not ok" 1 - "libwebp decode failed for invalid ICC input (cms=1)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_noicc}" "${output_invalid_icc}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "cms=1 keeps libwebp output unchanged for invalid ICC chunk"
exit 0
