#!/bin/sh
# TAP test ensuring img2sixel help command executes successfully.

# Enable strict mode with verbose tracing for diagnostics.
set -eux



script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



echo "1..1"
set -v

if run_img2sixel -H >"${ARTIFACT_LOCAL_DIR}/help.txt"; then
    pass 1 "help output available"
else
    fail 1 "help output failed"
fi

exit "${status}"
