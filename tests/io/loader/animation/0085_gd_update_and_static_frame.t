#!/bin/sh
# TAP test: gd loader accepts combined update and static frame options.

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

run_img2sixel -L gd! -ldisable -dnone -u -g \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null || {
    fail 1 "gd combined update/static frame failed"
    exit 0
}

pass 1 "gd combined update/static frame succeeded"
exit 0
