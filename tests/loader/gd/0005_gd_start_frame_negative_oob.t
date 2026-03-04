#!/bin/sh
# TAP test: gd loader rejects out-of-range negative start frame indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR is unavailable in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v

run_img2sixel --start-frame=-999 \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    echo "not ok" 1 "gd negative out-of-range start frame unexpectedly succeeded"
    exit 0
}

echo "ok" 1 "gd negative out-of-range start frame is rejected"
exit 0
