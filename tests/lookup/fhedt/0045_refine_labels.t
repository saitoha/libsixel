#!/bin/sh
# Verify refined labels.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0007_refine_labels || {
    printf '\n%s\n' "not ok 1 - byte FHEDT refined labels"
    exit 0
}

printf '\n%s\n' "ok 1 - byte FHEDT refined labels"
exit 0
