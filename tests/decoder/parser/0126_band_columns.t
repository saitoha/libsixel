#!/bin/sh
# Policy: docs/decoder/pipeline/README.md
# Band-column output expands to the RGBA decode of the same stream.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0112_band_columns" || {
    printf '\n%s\n' "not ok 1 - band columns expand to the RGBA decode"
    exit 0
}

printf '\n%s\n' "ok 1 - band columns expand to the RGBA decode"
exit 0
