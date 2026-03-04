#!/bin/sh
# TAP test: quicklook decodes progressive JPEG(4:2:0) with stable quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

test "${SIXEL_TEST_HOST_ARCH-}" != "x86_64" || {
    printf "1..0 # SKIP quicklook coverage is unstable on x86_64 for this input\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-progressive-420.jpg" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_jpeg_progressive_420.six" || {
    echo "not ok" 1 "quicklook progressive JPEG(4:2:0) decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_jpeg_progressive_420.six" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "quicklook progressive JPEG(4:2:0) decode preserves quality"
exit 0
