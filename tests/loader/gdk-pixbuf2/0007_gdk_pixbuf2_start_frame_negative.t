#!/bin/sh
# TAP test: gdk-pixbuf2 animation start frame accepts negative indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

run_img2sixel -L gdk-pixbuf2! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gdk_start_default_neg.six" || {
    echo "not ok" 1 - "baseline gdk-pixbuf2 animation decode failed"
    exit 0
}

run_img2sixel --start-frame=-1 \
    -L gdk-pixbuf2! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >"${ARTIFACT_LOCAL_DIR}/gdk_start_negative.six" || {
    echo "not ok" 1 - "gdk-pixbuf2 decode with negative start frame failed"
    exit 0
}

cmp -s "${ARTIFACT_LOCAL_DIR}/gdk_start_default_neg.six" \
    "${ARTIFACT_LOCAL_DIR}/gdk_start_negative.six" && {
    echo "not ok" 1 - "negative start frame did not change gdk-pixbuf2 output"
    exit 0
}

echo "ok" 1 - "gdk-pixbuf2 negative start frame is applied"
exit 0
