#!/bin/sh
# TAP test: builtin loader accepts APNG invalid blend operation input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_blend2.png" -o/dev/null || {
    fail 1 "APNG invalid blend_op decode failed on builtin loader"
    exit 0
}

pass 1 "APNG invalid blend_op input is accepted by builtin loader"
exit 0
