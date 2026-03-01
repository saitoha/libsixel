#!/bin/sh
# TAP test: gd static frame rendering with animation flags succeeds.

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

run_img2sixel -L gd! -ldisable -dnone -g \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null || {
    echo "not ok" 1 "gd static frame rendering failed"
    exit 0
}

echo "ok" 1 "gd static frame rendering succeeded"
exit 0
