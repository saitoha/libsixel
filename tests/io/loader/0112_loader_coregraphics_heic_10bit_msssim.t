#!/bin/sh
# TAP test: coregraphics decodes 10-bit HEIC content with stable quality.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-heic-10bit-lossless-64.heic" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_heic_10bit_lossless.six" || {
    fail 1 "coregraphics 10-bit HEIC decode failed"
    exit 0
}

lsqa_msg=$(set +x; run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/heif-10bit-gradient-reference.png" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_heic_10bit_lossless.six" 2>&1) || {
    fail 1 "$lsqa_msg"
    exit 0
}

pass 1 "coregraphics 10-bit HEIC decode preserves quality"
exit 0
