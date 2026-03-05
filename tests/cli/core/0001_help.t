#!/bin/sh
# TAP test ensuring img2sixel help command executes successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -H >/dev/null || {
    echo "not ok" 1 "help output failed"
    exit 0
}

echo "ok" 1 "help output available"
exit 0
