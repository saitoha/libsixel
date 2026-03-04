#!/bin/sh
# TAP test: gd loader animation start frame accepts negative indexes.

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

run_img2sixel --start-frame=-1 \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gd_start_negative.six" || {
    echo "not ok" 1 "gd decode with negative start frame failed"
    exit 0
}

run_img2sixel --start-frame=4 \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gd_start_positive_equivalent.six" || {
    echo "not ok" 1 "gd decode with equivalent positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/gd_start_negative.six" \
    "${ARTIFACT_LOCAL_DIR}/gd_start_positive_equivalent.six" || {
    echo "not ok" 1 "negative start frame did not map to last gd frame"
    exit 0
}

echo "ok" 1 "gd negative start frame resolves from tail"
exit 0
