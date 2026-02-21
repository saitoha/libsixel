#!/bin/sh
# TAP test: builtin loader accepts APNG fcTL sequence gap input.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fctl_sequence_gap.png" \
    -o/dev/null || {
    fail 1 "APNG fcTL sequence gap decode failed on builtin loader"
    exit 0
}

pass 1 "APNG fcTL sequence gap input is accepted by builtin loader"
exit 0
