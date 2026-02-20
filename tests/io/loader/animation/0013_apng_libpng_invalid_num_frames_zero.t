#!/bin/sh
# TAP test: APNG rejects acTL num_frames of zero.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel -Llibpng! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_num_frames_zero.png" -o/dev/null && {
    fail 1 "APNG num_frames zero unexpectedly succeeded"
    exit 0
}

pass 1 "APNG num_frames zero is rejected"
exit 0

