#!/bin/sh
# TAP test verifying sixel2png prints help output.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_sixel2png -H >/dev/null || {
    echo "not ok" 1 - "help option failed"
    exit 0
}

echo "ok" 1 - "prints help"
exit 0
