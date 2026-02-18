#!/bin/sh
# TAP test ensuring high-color option and palette size options conflict as expected.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -I -p8 </dev/null >/dev/null  && {
    fail 1 "unexpected success: high-color option and palette size options conflict"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
