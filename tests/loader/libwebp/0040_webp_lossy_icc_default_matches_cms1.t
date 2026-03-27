#!/bin/sh
# Verify lossy WebP ICC input matches between default cms and explicit cms=0.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64_embedded_a98_icc.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-lossy-icc-default.sixel"
output_cms0="${ARTIFACT_LOCAL_DIR}/webp-lossy-icc-cms0-default-check.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp! "${input_webp}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp lossy ICC decode failed with default cms"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_cms0}" || {
    echo "not ok" 1 - "libwebp lossy ICC decode failed with cms=0"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_cms0}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "lossy ICC default cms behavior matches explicit cms=0"
exit 0
