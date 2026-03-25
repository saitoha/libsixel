#!/bin/sh
# Verify libwebp cms=1 treats valid and invalid ICC payloads differently.

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

input_webp_valid_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc.webp"
input_webp_invalid_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_invalid_icc.webp"
output_valid_icc="${ARTIFACT_LOCAL_DIR}/webp-valid-icc-cms1.sixel"
output_invalid_icc="${ARTIFACT_LOCAL_DIR}/webp-invalid-icc-cms1-diff.sixel"

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp_valid_icc}" >"${output_valid_icc}" || {
    echo "not ok" 1 - "libwebp decode failed for valid ICC input (cms=1)"
    exit 0
}

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp_invalid_icc}" >"${output_invalid_icc}" || {
    echo "not ok" 1 - "libwebp decode failed for invalid ICC input (cms=1)"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_valid_icc}" "${output_invalid_icc}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "valid and invalid ICC payloads were not distinguishable: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "cms=1 distinguishes valid ICC conversion from invalid ICC payload"
exit 0
