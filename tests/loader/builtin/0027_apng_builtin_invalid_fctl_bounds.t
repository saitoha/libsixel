#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png.md
# TAP test: an early out-of-bounds APNG frame uses static fallback.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_TRACE_TOPIC=encode_handoff,apng_decode,lifecycle -Lbuiltin! "${TOP_SRCDIR}/tests/data/inputs/formats/apng_invalid_fctl_oob.png" -o/dev/null || {
    echo "not ok" 1 - "APNG out-of-bounds frame did not recover as static"
    exit 0
}

echo "ok" 1 - "early APNG out-of-bounds frame recovers as static"
exit 0
