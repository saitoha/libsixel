#!/bin/sh
# TAP test: builtin APNG start frame accepts positive indexes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_default.six" || {
    fail 1 "baseline APNG decode failed"
    exit 0
}

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >"${ARTIFACT_LOCAL_DIR}/apng_start_positive.six" || {
    fail 1 "APNG decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/apng_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/apng_start_positive.six" && {
    fail 1 "positive start frame did not change static APNG output"
    exit 0
}

pass 1 "builtin APNG positive start frame is applied"
exit 0
