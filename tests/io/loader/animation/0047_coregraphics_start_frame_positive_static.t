#!/bin/sh
# TAP test: coregraphics static decode applies positive start frame.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! -ldisable -S \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_static_start_default.six" || {
    fail 1 "baseline static coregraphics decode failed"
    exit 0
}

run_img2sixel --start-frame=1 \
    -L coregraphics! -ldisable -S \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_static_start_positive.six" || {
    fail 1 "static coregraphics decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/coregraphics_static_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_static_start_positive.six" && {
    fail 1 "positive start frame did not change static coregraphics output"
    exit 0
}

pass 1 "static coregraphics positive start frame is applied"
exit 0
