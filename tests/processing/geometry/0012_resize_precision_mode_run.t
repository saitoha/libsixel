#!/bin/sh
# TAP test: img2sixel runs with resize planner verbose dump enabled.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -v -=1 -W oklab -w 99% -o/dev/null << 'PPM' || {
P3
4 4
255
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
255 0 0   0 255 0   0 0 255   255 255 0
PPM
    echo "not ok" 1 "img2sixel failed with verbose dump"
    exit 0
}

echo "ok" 1 "img2sixel completed with verbose dump"

exit 0
