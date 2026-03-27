#!/bin/sh
# TAP test: APNG fdAT without fcTL input is rejected by builtin loader path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_libpng_fdat_without_fctl.png" \
    -o/dev/null && {
    echo "not ok" 1 - "APNG fdAT without fcTL unexpectedly succeeded"
    exit 0
}

echo "ok" 1 - "APNG fdAT without fcTL is rejected"
exit 0
