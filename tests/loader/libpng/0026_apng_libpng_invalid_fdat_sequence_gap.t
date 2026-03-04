#!/bin/sh
# TAP test: APNG rejects frame data sequence gaps on libpng path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Llibpng! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fdat_sequence_gap.png" \
    -o/dev/null && {
    echo "not ok" 1 "APNG fdAT sequence gap unexpectedly succeeded"
    exit 0
}

echo "ok" 1 "APNG fdAT sequence gap is rejected"
exit 0
