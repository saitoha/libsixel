#!/bin/sh
# TAP test: gd static frame rendering with animation flags succeeds.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_GD-}" = 1 || {
    printf "1..0 # SKIP gd support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

run_img2sixel -L gd! -ldisable -dnone -g \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" >/dev/null || {
    fail 1 "gd static frame rendering failed"
    exit 0
}

pass 1 "gd static frame rendering succeeded"
exit 0
