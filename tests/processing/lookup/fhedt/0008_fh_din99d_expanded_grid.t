#!/bin/sh
# Verify fhedt din99d expanded grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0008_fh_din99d_expanded_grid || {
    echo "not ok 1 - fhedt din99d expanded grid"
    exit 0
}

echo "ok 1 - fhedt din99d expanded grid"
exit 0
