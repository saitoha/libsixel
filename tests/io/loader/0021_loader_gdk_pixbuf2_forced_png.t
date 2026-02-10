#!/bin/sh
# TAP test confirming --loaders gdk-pixbuf2! forces PNG decoding path.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_feature_available "HAVE_GDK_PIXBUF2" "" "gdk-pixbuf2 loader"

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"

run_img2sixel -L gdk-pixbuf2! "${input_png}" >/dev/null || {
    fail 1 "gdk-pixbuf2 forced PNG decoding failed"
    exit 0
}

pass 1 "gdk-pixbuf2 forced PNG decoding succeeds"
exit 0
