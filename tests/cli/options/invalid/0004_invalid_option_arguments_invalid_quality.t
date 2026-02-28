#!/bin/sh
# TAP test ensuring img2sixel rejects invalid quality presets.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -q "" </dev/null >/dev/null  && {
    fail 1 "unexpected success: invalid findtype name"
    exit 0
}

pass 1 "invalid option rejected"
exit 0
