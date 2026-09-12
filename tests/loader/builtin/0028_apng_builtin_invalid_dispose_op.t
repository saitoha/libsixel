#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# TAP test: an early invalid APNG dispose operation uses static fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_dispose3.png" -o/dev/null || {
    echo "not ok" 1 - "APNG invalid dispose_op did not recover as static"
    exit 0
}

echo "ok" 1 - "early APNG invalid dispose_op recovers as static"
exit 0
