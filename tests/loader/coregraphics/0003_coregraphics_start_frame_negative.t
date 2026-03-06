#!/bin/sh
# TAP test: coregraphics animation start frame accepts negative indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

run_img2sixel --start-frame=-1 \
    -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_start_negative.six" || {
    echo "not ok" 1 - "coregraphics decode with negative start frame failed"
    exit 0
}

run_img2sixel --start-frame=4 \
    -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_start_positive_equivalent.six" || {
    echo "not ok" 1 - "coregraphics decode with equivalent positive frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/coregraphics_start_negative.six" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_start_positive_equivalent.six" || {
    echo "not ok" 1 - "negative start frame did not map to last coregraphics frame"
    exit 0
}

echo "ok" 1 - "coregraphics negative start frame resolves from tail"
exit 0
