#!/bin/sh
# TAP test ensuring img2sixel help command executes successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

run_img2sixel -H >"${ARTIFACT_LOCAL_DIR}/help.txt" || {
    fail 1 "help output failed"
    exit 0
}

pass 1 "help output available"
exit 0
