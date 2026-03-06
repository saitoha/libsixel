#!/bin/sh
# TAP test: coregraphics static baseline decode output is non-empty.

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

run_img2sixel -L coregraphics! -ldisable -S \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_static_start_default.six" || {
    echo "not ok" 1 - "baseline static coregraphics decode failed"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/coregraphics_static_start_default.six" || {
    echo "not ok" 1 - "baseline static coregraphics decode produced empty output"
    exit 0
}

echo "ok" 1 - "baseline static coregraphics output is non-empty"
exit 0
