#!/bin/sh
# Verify exhaustive discrete grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0003_grid || {
    echo "not ok 1 - byte FHEDT exhaustive discrete grid"
    exit 0
}

echo "ok 1 - byte FHEDT exhaustive discrete grid"
exit 0
