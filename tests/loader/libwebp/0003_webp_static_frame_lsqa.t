#!/bin/sh
# TAP test: libwebp -S renders the first frame for tiny animation.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

test -n "${LSQA_PATH-}" || {
    printf "1..0 # SKIP lsqa path is unavailable\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

image_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp"
image_ref="${TOP_SRCDIR}/tests/data/inputs/formats/animated-8x8-frame1-reference.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/webp_static_frame.six"

run_img2sixel -Llibwebp! -S "${image_webp}" -o "${output_sixel}" || {
    echo "not ok" 1 "libwebp static frame conversion failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" "${image_ref}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 "${lsqa_msg}"
    exit 0
}

echo "ok" 1 "libwebp static frame output matched expected first frame"
exit 0
