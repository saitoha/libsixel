#!/bin/sh
# TAP test: coregraphics interlaced GIF static output differs from default output.

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

run_img2sixel -L coregraphics! -ldisable -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" \
    >"${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_static.six" || {
    fail 1 "static interlaced GIF decode failed on coregraphics"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_default.six" \
    "${ARTIFACT_LOCAL_DIR}/coregraphics_interlaced_static.six" && {
    fail 1 "default and static coregraphics interlaced GIF outputs match"
    exit 0
}

pass 1 "coregraphics interlaced GIF static output differs from default output"
exit 0
