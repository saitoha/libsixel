#!/bin/sh
# TAP test: img2sixel runs with resize planner verbose dump enabled.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

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
    fail 1 "img2sixel failed with verbose dump"
    exit 0
}

pass 1 "img2sixel completed with verbose dump"

exit 0
