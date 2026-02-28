#!/bin/sh
# TAP test: coregraphics handles disable/update flag combination.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L coregraphics! -ldisable -dnone -u \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" -o/dev/null || {
    fail 1 "coregraphics disable/update flag combination failed"
    exit 0
}

pass 1 "coregraphics disable/update flag combination succeeded"
exit 0
