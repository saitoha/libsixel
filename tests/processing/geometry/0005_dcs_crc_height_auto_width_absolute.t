#!/bin/sh
# Check DCS coordinates for geometry scaling combinations.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

expected="3871514854 39"
sum=$(printf '%b' '\033Pq"1;1;1;1!6~\033\057' | run_img2sixel -=1 -rne -hauto -w12 | cksum)

test -n "${sum}" || {
    echo "not ok" 1 - "DCS coordinates stayed consistent (no payload produced)"
    exit 0
}

test "${sum}" = "${expected}" || {
    echo "not ok" 1 - "DCS coordinates stayed consistent"
    exit 0
}

echo "ok" 1 - "DCS coordinates stayed consistent"
exit 0
