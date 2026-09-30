#!/bin/sh
# Verify refined labels.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0007_refine_labels || {
    echo "not ok 1 - byte FHEDT refined labels"
    exit 0
}

echo "ok 1 - byte FHEDT refined labels"
exit 0
