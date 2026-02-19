#!/bin/sh
# TAP test verifying install-completion rejects missing targets.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"


test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo '1..1'
set -v

run_img2sixel -2 >/dev/null && {
    fail 1 "missing install target unexpectedly succeeded"
    exit 0
}

pass 1 "missing install target is rejected"
exit 0
