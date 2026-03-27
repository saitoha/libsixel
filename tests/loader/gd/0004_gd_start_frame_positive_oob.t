#!/bin/sh
# TAP test: gd loader rejects out-of-range positive start frame indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


test "${HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR-}" = 1 || {
    printf "1..0 # SKIP HAVE_DECL_GDIMAGECREATEFROMGIFANIMPTR is unavailable in this build\n";
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --start-frame=999 \
    -L gd! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    echo "not ok" 1 - "gd positive out-of-range start frame unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "gd positive out-of-range start frame is rejected"
exit 0
