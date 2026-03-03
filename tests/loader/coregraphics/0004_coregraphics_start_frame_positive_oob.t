#!/bin/sh
# TAP test: coregraphics rejects out-of-range positive frame indexes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics support is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel --start-frame=999 \
    -L coregraphics! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null && {
    echo "not ok" 1 "coregraphics positive out-of-range start frame succeeded"
    exit 0
}

echo "ok" 1 "coregraphics positive out-of-range start frame is rejected"
exit 0
