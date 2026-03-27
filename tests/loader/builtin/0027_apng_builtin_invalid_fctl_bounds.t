#!/bin/sh
# TAP test: builtin loader accepts APNG out-of-bounds frame rectangles.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_fctl_oob.png" -o/dev/null || {
    echo "not ok" 1 - "APNG out-of-bounds frame rect decode failed on builtin loader"
    exit 0
}

echo "ok" 1 - "APNG out-of-bounds frame rect input is accepted by builtin loader"
exit 0
