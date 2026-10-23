#!/bin/sh
# Verify 16-bit labels and duplicate seeds.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_THREADING_POLICY=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0005_u16_labels || {
    echo "not ok 1 - byte FHEDT 16-bit labels and duplicate seeds"
    exit 0
}

echo "ok 1 - byte FHEDT 16-bit labels and duplicate seeds"
exit 0
