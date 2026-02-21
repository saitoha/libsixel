#!/bin/sh
# TAP test: builtin loader accepts APNG acTL num_frames mismatch input.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_num_frames_mismatch.png" -o/dev/null || {
    fail 1 "APNG num_frames mismatch decode failed on builtin loader"
    exit 0
}

pass 1 "APNG num_frames mismatch input is accepted by builtin loader"
exit 0
