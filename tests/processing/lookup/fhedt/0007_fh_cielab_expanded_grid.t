#!/bin/sh
# Verify fhedt cielab expanded grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0007_fh_cielab_expanded_grid || {
    echo "not ok 1 - fhedt cielab expanded grid"
    exit 0
}

echo "ok 1 - fhedt cielab expanded grid"
exit 0
