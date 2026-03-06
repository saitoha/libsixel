#!/bin/sh
# TAP test: gd loader animation start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR is unavailable in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

run_img2sixel -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gd_start_default.six" || {
    echo "not ok" 1 - "baseline gd animation decode failed"
    exit 0
}

run_img2sixel --start-frame=1 \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gd_start_positive.six" || {
    echo "not ok" 1 - "gd decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/gd_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/gd_start_positive.six" && {
    echo "not ok" 1 - "positive start frame did not change gd output"
    exit 0
}

echo "ok" 1 - "gd positive start frame is applied"
exit 0
