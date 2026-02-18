#!/bin/sh
# TAP test ensuring img2sixel help command executes successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -H >"${ARTIFACT_LOCAL_DIR}/help.txt" || {
    fail 1 "help output failed"
    exit 0
}

pass 1 "help output available"
exit 0
