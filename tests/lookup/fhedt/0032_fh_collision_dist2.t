#!/bin/sh
# Verify fhedt collision dist2.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0022_fh_collision_dist2 || {
    echo "not ok 1 - fhedt collision dist2"
    exit 0
}

echo "ok 1 - fhedt collision dist2"
exit 0
