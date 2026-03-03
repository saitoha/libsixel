#!/bin/sh
# TAP test: coregraphics animated GIF default decode output is non-empty.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -v -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_animated_default.six" || {
    echo "not ok" 1 "default animated GIF decode failed on coregraphics"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/coregraphics_animated_default.six" || {
    echo "not ok" 1 "default animated GIF decode produced empty output"
    exit 0
}

echo "ok" 1 "coregraphics emits non-empty output for animated GIF"
exit 0
