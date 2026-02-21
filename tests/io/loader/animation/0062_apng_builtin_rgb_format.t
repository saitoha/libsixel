#!/bin/sh
# TAP test: APNG RGB pixel format decode succeeds.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" -o/dev/null || {
    fail 1 "APNG RGB decode failed"
    exit 0
}

pass 1 "APNG RGB decode succeeds"
exit 0

