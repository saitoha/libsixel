#!/bin/sh
# Verify default cms target output matches explicit linear target for WebP ICC.

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
output_default="${ARTIFACT_LOCAL_DIR}/webp-icc-target-default.sixel"
output_linear="${ARTIFACT_LOCAL_DIR}/webp-icc-target-linear.sixel"

run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp ICC decode failed with default cms target"
    exit 0
}

SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    run_img2sixel -Llibwebp:cms_engine=auto! "${input_webp}" >"${output_linear}" || {
    echo "not ok" 1 - "libwebp ICC decode failed with cms target linear"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_linear}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "default cms target matches explicit linear target for ICC decode"
exit 0
