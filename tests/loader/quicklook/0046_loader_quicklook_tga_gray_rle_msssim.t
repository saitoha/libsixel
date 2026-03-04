#!/bin/sh
# TAP test: quicklook decodes grayscale TGA(type11) with stable quality.

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

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type11-gray.tga" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_tga_type11_gray.six" || {
    echo "not ok" 1 "quicklook grayscale TGA(type11) decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.ppm" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_tga_type11_gray.six" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "quicklook grayscale TGA(type11) decode preserves quality"
exit 0
