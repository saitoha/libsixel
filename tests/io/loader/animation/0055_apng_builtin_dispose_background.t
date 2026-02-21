#!/bin/sh
# TAP test: APNG dispose background is accepted.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_dispose_background.png" -o/dev/null || {
    fail 1 "APNG dispose background failed"
    exit 0
}

pass 1 "APNG dispose background succeeds"
exit 0

