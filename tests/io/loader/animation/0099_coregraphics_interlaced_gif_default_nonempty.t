#!/bin/sh
# TAP test: coregraphics interlaced GIF default decode output is non-empty.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_default.six" || {
    fail 1 "default interlaced GIF decode failed on coregraphics"
    exit 0
}

test -s "${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_default.six" || {
    fail 1 "default interlaced GIF decode produced empty output"
    exit 0
}

pass 1 "coregraphics emits non-empty output for interlaced GIF"
exit 0
