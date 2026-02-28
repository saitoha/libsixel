#!/bin/sh
# TAP test: gdk-pixbuf2 animation start frame accepts positive indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gdk-pixbuf2! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gdk_start_default.six" || {
    echo "not ok" 1 "baseline gdk-pixbuf2 animation decode failed"
    exit 0
}

run_img2sixel --start-frame=1 \
    -L gdk-pixbuf2! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gdk_start_positive.six" || {
    echo "not ok" 1 "gdk-pixbuf2 decode with positive start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/gdk_start_default.six" \
    "${ARTIFACT_LOCAL_DIR}/gdk_start_positive.six" && {
    echo "not ok" 1 "positive start frame did not change gdk-pixbuf2 output"
    exit 0
}

echo "ok" 1 "gdk-pixbuf2 positive start frame is applied"
exit 0
