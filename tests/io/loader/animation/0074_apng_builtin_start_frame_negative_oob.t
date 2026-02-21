#!/bin/sh
# TAP test: builtin loader accepts negative out-of-range APNG start frame.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env "SIXEL_LOADER_ANIMATION_START_FRAME_NO=-999" \
    -Lbuiltin! -S \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgb_loop2.png" \
    >/dev/null || {
    fail 1 "out-of-range negative start frame decode failed on builtin loader"
    exit 0
}

pass 1 "builtin loader accepts negative out-of-range start frame"
exit 0
