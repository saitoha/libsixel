#!/bin/sh
# TAP test: APNG RGBA pixel format decode succeeds.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Llibpng! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" -o/dev/null || {
    fail 1 "APNG RGBA decode failed"
    exit 0
}

pass 1 "APNG RGBA decode succeeds"
exit 0

