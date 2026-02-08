#!/bin/sh
# TAP test confirming --loaders gdk-pixbuf2! forces PNG decoding path.

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

input_png="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_forced_png.sixel"

if run_img2sixel -L gdk-pixbuf2! "${input_png}" >"${output_sixel}"; then
    pass ${case_id} "gdk-pixbuf2 forced PNG decoding succeeds"
else
    fail ${case_id} "gdk-pixbuf2 forced PNG decoding failed"
fi

exit "${status}"
