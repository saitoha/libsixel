#!/bin/sh
# TAP test confirming forced gdk-pixbuf2 decoding rejects corrupted PNG input.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_feature_available "HAVE_GDK_PIXBUF2" "" "gdk-pixbuf2 loader"

echo "1..1"
set -v

input_png="${top_srcdir}/tests/data/corrupted/truncated.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_corrupted_png.sixel"

run_img2sixel -L gdk-pixbuf2! "${input_png}" >"${output_sixel}" && {
    fail 1 "forced gdk-pixbuf2 corrupted PNG should fail"
    exit 0
}

pass 1 "forced gdk-pixbuf2 corrupted PNG is rejected"

exit 0
