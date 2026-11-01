#!/bin/sh
# Verify fhedt collision refine.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0021_fh_collision_refine || {
    echo "not ok 1 - fhedt collision refine"
    exit 0
}

echo "ok 1 - fhedt collision refine"
exit 0
