#!/bin/sh
# Verify weighted discrete grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0004_weighted_grid || {
    printf '\n%s\n' "not ok 1 - byte FHEDT weighted discrete grid"
    exit 0
}

printf '\n%s\n' "ok 1 - byte FHEDT weighted discrete grid"
exit 0
