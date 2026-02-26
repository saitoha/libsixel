#!/bin/sh
# TAP test: quicklook decodes 4:4:4 JPEG with stable visual quality.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_QUICKLOOK-}" = 1 || {
    printf "1..0 # SKIP quicklook loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_THUMBNAILER_HINT_SIZE=64 -L quicklook! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-444.jpg" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_jpeg_444.six" || {
    fail 1 "quicklook JPEG decode failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_jpeg_444.six" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "quicklook JPEG decode preserves quality"
exit 0
