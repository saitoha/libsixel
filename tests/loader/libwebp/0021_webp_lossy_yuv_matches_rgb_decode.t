#!/bin/sh
# Verify lossy WebP YUV decode path stays close to legacy RGB decode output.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/snake_64.webp"
output_yuv="${ARTIFACT_LOCAL_DIR}/webp-lossy-yuv-path.sixel"
output_rgb="${ARTIFACT_LOCAL_DIR}/webp-lossy-rgb-path.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_yuv}" || {
    echo "not ok" 1 - "libwebp lossy YUV-path decode failed"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=none! "${input_webp}" >"${output_rgb}" || {
    echo "not ok" 1 - "libwebp lossy RGB-path decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.995" "${output_yuv}" "${output_rgb}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "lossy YUV decode matches legacy RGB decode closely"
exit 0
