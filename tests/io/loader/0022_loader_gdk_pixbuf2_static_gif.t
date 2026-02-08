#!/bin/sh
# TAP test confirming --static forces single-frame GIF decode on gdk-pixbuf2.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_feature_available "HAVE_GDK_PIXBUF2" "" "gdk-pixbuf2 loader"

echo "1..1"
set -v

input_gif="${top_srcdir}/tests/data/inputs/small.gif"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_static_gif.sixel"

if run_img2sixel -L gdk-pixbuf2! -S "${input_gif}" >"${output_sixel}"; then
    pass ${case_id} "gdk-pixbuf2 static GIF decode succeeds"
else
    fail ${case_id} "gdk-pixbuf2 static GIF decode failed"
fi

exit "${status}"
