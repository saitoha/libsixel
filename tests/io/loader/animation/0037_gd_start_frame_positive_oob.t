#!/bin/sh
# TAP test: gd loader rejects out-of-range positive start frame indexes.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_GD-}" = 1 || {
    printf "1..0 # SKIP gd support is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --start-frame=999 \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    fail 1 "gd positive out-of-range start frame unexpectedly succeeded"
    exit 0
}

pass 1 "gd positive out-of-range start frame is rejected"
exit 0
