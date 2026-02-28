#!/bin/sh
# TAP test: builtin loader accepts APNG acTL num_frames of zero.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_num_frames_zero.png" -o/dev/null || {
    echo "not ok" 1 "APNG num_frames zero decode failed on builtin loader"
    exit 0
}

echo "ok" 1 "APNG num_frames zero input is accepted by builtin loader"
exit 0
