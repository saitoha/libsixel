#!/bin/sh
# TAP test for issue #167 empty background argument handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"


printf '1..1\n'
set -v

run_img2sixel -B '#000' -B '' </dev/null >/dev/null && {
    fail 1 "empty background accepted unexpectedly"
    exit 0
}

test "$?" = 2 || {
    fail 1 "empty background is not rejected"
    exit 0
}

pass 1 "empty background rejected"
exit 0
