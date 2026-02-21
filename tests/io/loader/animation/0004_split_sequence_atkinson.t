#!/bin/sh
# TAP test: static frame with Atkinson dithering from animated GIF matches reference.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! -S -datkinson \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/small_gif_atkinson_static.six" || {
    fail 1 "sequence splitting with Atkinson fails"
    exit 0
}

lsqa_msg=$(run_lsqa -m MS-SSIM -b "MS-SSIM:0.98" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/small_gif_atkinson_static_reference.six" \
    "${ARTIFACT_LOCAL_DIR}/small_gif_atkinson_static.six" 2>&1) || {
    fail 1 "${lsqa_msg}"
    exit 0
}

pass 1 "sequence splitting with Atkinson matches static reference"
exit 0
