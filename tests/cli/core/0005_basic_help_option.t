#!/bin/sh
# TAP test verifying sixel2png prints help output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || skip_all "sixel2png is disabled in this build"

echo "1..1"
set -v

run_sixel2png -H >"${ARTIFACT_LOCAL_DIR}/help.txt" || {
    fail 1 "help option failed"
    exit 0
}

pass 1 "prints help"
exit 0
