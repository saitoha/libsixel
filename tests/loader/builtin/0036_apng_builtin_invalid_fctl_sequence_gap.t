#!/bin/sh
# TAP test: builtin loader accepts APNG fcTL sequence gap input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fctl_sequence_gap.png" \
    -o/dev/null || {
    echo "not ok" 1 - "APNG fcTL sequence gap decode failed on builtin loader"
    exit 0
}

echo "ok" 1 - "APNG fcTL sequence gap input is accepted by builtin loader"
exit 0
