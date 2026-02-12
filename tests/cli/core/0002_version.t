#!/bin/sh
# TAP test ensuring img2sixel version command executes successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

run_img2sixel -V >"${ARTIFACT_LOCAL_DIR}/version.txt" || {
    fail 1 "version output failed"
    exit 0
}

pass 1 "version output available"
exit 0
