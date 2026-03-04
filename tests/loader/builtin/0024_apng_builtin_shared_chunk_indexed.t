#!/bin/sh
# TAP test: APNG shared PLTE and tRNS chunks are accepted.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_indexed_loop2.png" -o/dev/null || {
    echo "not ok" 1 "APNG indexed shared chunk failed"
    exit 0
}

echo "ok" 1 "APNG indexed shared chunk succeeds"
exit 0

