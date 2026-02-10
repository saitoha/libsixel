#!/bin/sh
# TAP test covering forced gdk-pixbuf2 decoding of SIXEL source input.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_feature_available "HAVE_GDK_PIXBUF2" "" "gdk-pixbuf2 loader"

echo "1..1"
set -v

input_sixel="${top_srcdir}/tests/data/inputs/snake_64.six"
error_log="${ARTIFACT_LOCAL_DIR}/gdk_forced_sixel_input.err"

if run_img2sixel -L gdk-pixbuf2! "${input_sixel}" >/dev/null \
        2>"${error_log}"; then
    pass 1 "gdk-pixbuf2 forced SIXEL input decoding succeeds"
else
    if grep -E "gdk|pixbuf|sixel|loader" "${error_log}" >/dev/null 2>&1; then
        tap_skip 1 "runtime gdk-pixbuf2 SIXEL subtype is unavailable"
    else
        fail 1 "forced gdk-pixbuf2 SIXEL input decoding failed"
    fi
fi

exit 0
