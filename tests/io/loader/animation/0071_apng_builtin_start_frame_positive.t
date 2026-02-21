#!/bin/sh
# TAP test: builtin APNG positive start frame matches static reference.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_positive.six" || {
    fail 1 "APNG decode with positive start frame failed"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2_builtin_start_frame_positive_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_positive.six" 2>&1) || {
    fail 1 "${lsqa_msg}"
    exit 0
}

pass 1 "builtin APNG positive start frame matches static reference"
exit 0
