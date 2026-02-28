#!/bin/sh
# TAP test: quicklook decodes HEIC 10bit lossless with stable quality.

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
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heic-10bit-lossless-64.heic" \
    >"${ARTIFACT_LOCAL_DIR}/quicklook_heic_10bit_lossless.six" || {
    fail 1 "quicklook HEIC 10bit lossless decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/heif-10bit-gradient-reference.png" \
    "${ARTIFACT_LOCAL_DIR}/quicklook_heic_10bit_lossless.six" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "quicklook HEIC 10bit lossless decode preserves quality"
exit 0
