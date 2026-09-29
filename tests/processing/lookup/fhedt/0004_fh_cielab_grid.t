#!/bin/sh
# Verify fhedt cielab grid.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0004_fh_cielab_grid || {
    echo "not ok 1 - fhedt cielab grid"
    exit 0
}

echo "ok 1 - fhedt cielab grid"
exit 0
