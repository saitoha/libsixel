#!/bin/sh
# Verify YUVA lossy ICC input with -B keeps YUV/RGB decode paths close (cms=1).

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossy-alpha-8x8-2frame-min-embedded-a98-icc.webp"
output_yuv="${ARTIFACT_LOCAL_DIR}/webp-yuva-icc-bg-yuv-cms1.sixel"
output_rgb="${ARTIFACT_LOCAL_DIR}/webp-yuva-icc-bg-rgb-cms1.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! -S -B#808080 "${input_webp}" >"${output_yuv}" || {
    echo "not ok" 1 - "libwebp YUVA ICC YUV-path decode with -B failed (cms=1)"
    exit 0
}

SIXEL_LOADER_LIBWEBP_LOSSY_USE_RGB_DECODE=1 \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp:cms_engine=auto! -S -B#808080 "${input_webp}" >"${output_rgb}" || {
    echo "not ok" 1 - "libwebp YUVA ICC RGB-path decode with -B failed (cms=1)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.995" "${output_yuv}" "${output_rgb}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "YUVA ICC + -B decode stays close between YUV and RGB paths with cms=1"
exit 0
