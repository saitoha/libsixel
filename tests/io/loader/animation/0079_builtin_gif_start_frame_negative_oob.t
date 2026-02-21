#!/bin/sh
# TAP test: builtin GIF negative out-of-range start frame returns an error.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=-999" \
    -L builtin! -ldisable \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" \
    >/dev/null && {
    fail 1 "out-of-range negative start frame unexpectedly succeeded"
    exit 0
}

pass 1 "builtin GIF negative out-of-range start frame is rejected"
exit 0
