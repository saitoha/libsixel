#!/bin/sh
# Verify thread-requested discrete grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP internal threads unavailable"
    set -v
    exit 0
}

echo "1..1"
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=2 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0006_threaded_grid || {
    printf '\n%s\n' "not ok 1 - byte FHEDT thread-requested discrete grid"
    exit 0
}

printf '\n%s\n' "ok 1 - byte FHEDT thread-requested discrete grid"
exit 0
