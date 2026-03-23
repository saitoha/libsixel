#!/bin/sh
# Verify lossy WebP ICC input differs between cms=1 and cms=0 decode.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
output_cms1="${ARTIFACT_LOCAL_DIR}/webp-lossy-icc-cms1.sixel"
output_cms0="${ARTIFACT_LOCAL_DIR}/webp-lossy-icc-cms0.sixel"

run_img2sixel -Llibwebp:cms=1! "${input_webp}" >"${output_cms1}" || {
    echo "not ok" 1 - "libwebp lossy ICC decode failed (cms=1)"
    exit 0
}

run_img2sixel -Llibwebp:cms=0! "${input_webp}" >"${output_cms0}" || {
    echo "not ok" 1 - "libwebp lossy ICC decode failed (cms=0)"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.995" "${output_cms1}" "${output_cms0}" 2>&1) || lsqa_status=$?

test "${lsqa_status-0}" -eq 5 || {
    echo "not ok" 1 - "lossy ICC cms=1 and cms=0 outputs were not distinguishable: ${lsqa_msg-}"
    exit 0
}

echo "ok" 1 - "lossy ICC input changes output when cms=1 is enabled"
exit 0
