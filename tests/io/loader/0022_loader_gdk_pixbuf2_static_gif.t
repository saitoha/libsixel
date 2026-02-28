#!/bin/sh
# TAP test confirming --static forces single-frame GIF decode on gdk-pixbuf2.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_static_gif.sixel"

run_img2sixel -L gdk-pixbuf2! -S "${input_gif}" >"${output_sixel}" || {
    echo "not ok" 1 "gdk-pixbuf2 static GIF decode failed"
    exit 0
}

echo "ok" 1 "gdk-pixbuf2 static GIF decode succeeds"
exit 0
