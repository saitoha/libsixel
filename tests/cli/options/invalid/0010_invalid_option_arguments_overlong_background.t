#!/bin/sh
# TAP test ensuring img2sixel rejects overlong background specifications.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -B "#0000000000000" </dev/null >/dev/null  && {
    fail 1 "unexpected success: invalid background specification"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
