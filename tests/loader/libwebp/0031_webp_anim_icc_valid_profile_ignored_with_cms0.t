#!/bin/sh
# Verify libwebp animation path ignores valid embedded ICC when cms=0.

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
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_webp_noicc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_noicc_anim2.webp"
input_webp_valid_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc_anim2.webp"
output_noicc="${ARTIFACT_LOCAL_DIR}/webp-anim-noicc-cms0.sixel"
output_valid_icc="${ARTIFACT_LOCAL_DIR}/webp-anim-valid-icc-cms0.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp_noicc}" >"${output_noicc}" || {
    echo "not ok" 1 - "libwebp animation decode failed for non-ICC input (cms=0)"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! -S "${input_webp_valid_icc}" >"${output_valid_icc}" || {
    echo "not ok" 1 - "libwebp animation decode failed for valid ICC input (cms=0)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_noicc}" "${output_valid_icc}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "animation cms=0 keeps output unchanged for valid ICC chunk"
exit 0
