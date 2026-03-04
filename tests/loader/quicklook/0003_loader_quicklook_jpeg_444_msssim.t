#!/bin/sh
# TAP test: quicklook decodes 4:4:4 JPEG with stable visual quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

test "${SIXEL_TEST_HOST_ARCH-}" != "x86_64" || {
    printf "1..0 # SKIP quicklook coverage is unstable on x86_64 for this input\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-444.jpg" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_jpeg_444.six" || {
    echo "not ok" 1 "quicklook JPEG decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_jpeg_444.six" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "quicklook JPEG decode preserves quality"
exit 0
