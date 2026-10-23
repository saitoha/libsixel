#!/bin/sh
# Verify weighted discrete grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_THREADING_POLICY=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt_byte/0004_weighted_grid || {
    echo "not ok 1 - byte FHEDT weighted discrete grid"
    exit 0
}

echo "ok 1 - byte FHEDT weighted discrete grid"
exit 0
