#!/bin/sh
# Verify fhedt din99d grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0005_fh_din99d_grid || {
    printf '\n%s\n' "not ok 1 - fhedt din99d grid"
    exit 0
}

printf '\n%s\n' "ok 1 - fhedt din99d grid"
exit 0
