#!/bin/sh
# TAP test: quicklook decodes RLE RGB TGA with stable visual quality.

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
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type10-rgb.tga" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_tga_rle_rgb.six" || {
    echo "not ok" 1 "quicklook TGA rle rgb decode failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_tga_rle_rgb.six" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "quicklook TGA rle rgb decode preserves quality"
exit 0
