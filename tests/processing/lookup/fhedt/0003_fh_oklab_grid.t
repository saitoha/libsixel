#!/bin/sh
# Verify fhedt oklab grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0003_fh_oklab_grid || {
    echo "not ok 1 - fhedt oklab grid"
    exit 0
}

echo "ok 1 - fhedt oklab grid"
exit 0
