#!/bin/sh
# Check DCS coordinates for geometry scaling combinations.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

expected="3871514854 39"
sum=$(printf '%b' '\033Pq"1;1;1;1!6~\033\057' | run_img2sixel -=1 -rne -w200% | cksum)

test -n "${sum}" || {
    fail 1 "width scaling preserves DCS coordinates (no payload produced)"
    exit 0
}

test "${sum}" = "${expected}" || {
    fail 1 "width scaling preserves DCS coordinates"
    exit 0
}

pass 1 "width scaling preserves DCS coordinates"
exit 0
