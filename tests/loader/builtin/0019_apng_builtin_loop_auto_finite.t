#!/bin/sh
# TAP test: APNG finite auto loop completes without hanging.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

run_img2sixel --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -v -Lbuiltin! -lauto "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" -o/dev/null || {
    echo "not ok" 1 - "APNG finite auto loop failed"
    exit 0
}

echo "ok" 1 - "APNG finite auto loop completes"
exit 0

