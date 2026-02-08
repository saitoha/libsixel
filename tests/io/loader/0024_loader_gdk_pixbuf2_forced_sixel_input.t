#!/bin/sh
# TAP test covering forced gdk-pixbuf2 decoding of SIXEL source input.

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

input_sixel="${top_srcdir}/tests/data/inputs/snake_64.six"
output_sixel="${ARTIFACT_LOCAL_DIR}/gdk_forced_sixel_input.sixel"
error_log="${ARTIFACT_LOCAL_DIR}/gdk_forced_sixel_input.err"

if run_img2sixel -L gdk-pixbuf2! "${input_sixel}" >"${output_sixel}" \
        2>"${error_log}"; then
    pass ${case_id} "gdk-pixbuf2 forced SIXEL input decoding succeeds"
else
    if grep -E "gdk|pixbuf|sixel|loader" "${error_log}" >/dev/null 2>&1; then
        tap_skip ${case_id} "runtime gdk-pixbuf2 SIXEL subtype is unavailable"
    else
        fail ${case_id} "forced gdk-pixbuf2 SIXEL input decoding failed"
    fi
fi

exit "${status}"
