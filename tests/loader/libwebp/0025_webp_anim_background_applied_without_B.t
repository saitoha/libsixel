#!/bin/sh
# Verify ANIM background is applied when -B is not specified.

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

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-alpha-8x8-2frame-min.webp"
output_default="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-default.six"
output_black="${ARTIFACT_LOCAL_DIR}/webp-anim-bg-black.six"

run_img2sixel -Llibwebp:cms=0! -S "${input_webp}" >"${output_default}" || {
    echo "not ok" 1 - "libwebp animation decode without -B failed"
    exit 0
}

run_img2sixel -Llibwebp:cms=0! -S -B#000 "${input_webp}" >"${output_black}" || {
    echo "not ok" 1 - "libwebp animation decode with -B#000 failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.999" "${output_default}" "${output_black}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "ANIM background is applied when -B is absent"
exit 0
