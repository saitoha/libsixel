#!/bin/sh
# Verify libwebp default cms behavior matches explicit cms=1 for animated ICC input.

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

input_webp_icc="${TOP_SRCDIR}/tests/data/inputs/formats/palette_lossless_embedded_a98_icc_anim2.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-anim-icc-default.sixel"
output_cms1="${ARTIFACT_LOCAL_DIR}/webp-anim-icc-cms1.sixel"

run_img2sixel -Llibwebp! -S "${input_webp_icc}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp default decode failed for animated ICC input"
    exit 0
}

run_img2sixel -Llibwebp:cms=1! -S "${input_webp_icc}" >"${output_cms1}" || {
    echo "not ok" 1 - "libwebp cms=1 decode failed for animated ICC input"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_cms1}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "libwebp default cms behavior matches explicit cms=1 for animated ICC input"
exit 0
