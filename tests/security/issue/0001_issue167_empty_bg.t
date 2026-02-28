#!/bin/sh
# TAP test for issue #167 empty background argument handling.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


printf '1..1\n'
set -v

run_img2sixel -B '#000' -B '' </dev/null >/dev/null && {
    echo "not ok" 1 "empty background accepted unexpectedly"
    exit 0
}

test "$?" = 2 || {
    echo "not ok" 1 "empty background is not rejected"
    exit 0
}

echo "ok" 1 "empty background rejected"
exit 0
