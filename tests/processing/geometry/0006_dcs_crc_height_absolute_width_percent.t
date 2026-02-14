#!/bin/sh
# Check DCS coordinates for geometry scaling combinations.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

expected="3871514854 39"
sum=$(printf '%b' '\033Pq"1;1;1;1!6~\033\\' | run_img2sixel -=1 -rne -h12 -w200% | cksum)

test -n "${sum}" || {
    fail 1 "DCS coordinates stayed consistent (no payload produced)"
    exit 0
}

test "${sum}" = "${expected}" || {
    fail 1 "DCS coordinates stayed consistent"
    exit 0
}

pass 1 "DCS coordinates stayed consistent"
exit 0
