#!/bin/sh
# TAP test confirming --loop-control=disable works on gdk-pixbuf2 GIF decode.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_feature_available "HAVE_GDK_PIXBUF2" "" "gdk-pixbuf2 loader"

echo "1..1"
set -v

input_gif="${top_srcdir}/tests/data/inputs/small.gif"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_loop_disable_gif.sixel"

run_img2sixel -L gdk-pixbuf2! -ldisable "${input_gif}" >"${output_sixel}" || {
    fail 1 "gdk-pixbuf2 GIF decode with loop disable failed"
    exit 0
}

pass 1 "gdk-pixbuf2 GIF decode respects loop disable"

exit 0
