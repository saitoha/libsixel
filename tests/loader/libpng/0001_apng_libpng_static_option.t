#!/bin/sh
# TAP test: APNG supports static render option.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Llibpng! -S "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" -o/dev/null || {
    echo "not ok" 1 "APNG static render failed"
    exit 0
}

echo "ok" 1 "APNG static render succeeds"
exit 0

