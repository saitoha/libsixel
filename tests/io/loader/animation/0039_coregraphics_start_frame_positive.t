#!/bin/sh
# TAP test: coregraphics animation start frame accepts positive indexes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_start_default.six" || {
    fail 1 "baseline coregraphics animation decode failed"
    exit 0
}

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=1" \
    -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_start_positive.six" || {
    fail 1 "coregraphics decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/coregraphics_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_start_positive.six" && {
    fail 1 "positive start frame did not change coregraphics output"
    exit 0
}

pass 1 "coregraphics positive start frame is applied"
exit 0
