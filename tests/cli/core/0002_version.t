#!/bin/sh
# TAP test ensuring img2sixel version command executes successfully.

# Enable strict mode with verbose tracing for diagnostics.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

if run_img2sixel -V >"${ARTIFACT_LOCAL_DIR}/version.txt"; then
    pass 1 "version output available"
else
    fail 1 "version output failed"
fi

exit "${status}"
