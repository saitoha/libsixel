#!/bin/sh
# TAP test: gd loader handles disable/update flag combination.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR is unavailable in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gd! -ldisable -dnone -u \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" -o/dev/null || {
    echo "not ok" 1 "gd disable/update flag combination failed"
    exit 0
}

echo "ok" 1 "gd disable/update flag combination succeeded"
exit 0
