#!/bin/sh
# Verify fhedt gamma expanded grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0006_fh_gamma_expanded_grid || {
    echo "not ok 1 - fhedt gamma expanded grid"
    exit 0
}

echo "ok 1 - fhedt gamma expanded grid"
exit 0
