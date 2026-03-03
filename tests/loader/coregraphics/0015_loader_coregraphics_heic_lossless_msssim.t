#!/bin/sh
# TAP test: coregraphics decodes lossless HEIC with stable visual quality.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heic-lossless-64.heic" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_heic_lossless.six" || {
    echo "not ok" 1 "coregraphics HEIC decode failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_heic_lossless.six" 2>&1) || {
    echo "not ok" 1 "$lsqa_msg"
    exit 0
}

echo "ok" 1 "coregraphics HEIC decode preserves quality"
exit 0
