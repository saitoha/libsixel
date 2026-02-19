#!/bin/sh
# TAP test verifying sixel2png prints version information.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_sixel2png -V >"${ARTIFACT_LOCAL_DIR}/version.txt" || {
    fail 1 "version option failed"
}

pass 1 "prints version"
exit 0
