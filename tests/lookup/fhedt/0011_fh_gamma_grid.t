#!/bin/sh
# Verify fhedt gamma grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0001_fh_gamma_grid || {
    printf '\n%s\n' "not ok 1 - fhedt gamma grid"
    exit 0
}

printf '\n%s\n' "ok 1 - fhedt gamma grid"
exit 0
