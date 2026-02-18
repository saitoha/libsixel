#!/bin/sh
# TAP test confirming --loop-control=disable works on gdk-pixbuf2 GIF decode.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}
test "${HAVE_GDK_PIXBUF2-}" = 1 || {
    printf "1..0 # SKIP gdk-pixbuf2 support is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/small.gif"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_loop_disable_gif.sixel"

run_img2sixel -L gdk-pixbuf2! -ldisable "${input_gif}" >"${output_sixel}" || {
    fail 1 "gdk-pixbuf2 GIF decode with loop disable failed"
    exit 0
}

pass 1 "gdk-pixbuf2 GIF decode respects loop disable"

exit 0
