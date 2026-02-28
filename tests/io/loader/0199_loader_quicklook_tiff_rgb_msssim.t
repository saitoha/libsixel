#!/bin/sh
# TAP test: quicklook decodes baseline RGB TIFF with stable quality.

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
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.tiff" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_tiff_rgb.six" || {
    fail 1 "quicklook baseline TIFF rgb decode failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_tiff_rgb.six" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "quicklook baseline TIFF rgb decode preserves quality"
exit 0
