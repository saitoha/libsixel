#!/bin/sh
# TAP test: builtin loader currently rejects APNG start frame override via -T.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=0" \
    -T 1 -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >/dev/null && {
    fail 1 "-T unexpectedly succeeded with builtin loader"
    exit 0
}

pass 1 "builtin loader rejects APNG -T start frame override"
exit 0
