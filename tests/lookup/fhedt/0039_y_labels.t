#!/bin/sh
# Verify Y-pass labels.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0001_y_labels || {
    echo "not ok 1 - byte FHEDT Y-pass labels"
    exit 0
}

echo "ok 1 - byte FHEDT Y-pass labels"
exit 0
