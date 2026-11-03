#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify cpu r64 sparse and colliding seed labels.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" fhedt/cpu_sparse_r64 || {
    echo "not ok 1 - cpu r64 sparse and colliding seed labels"
    exit 0
}
echo "ok 1 - cpu r64 sparse and colliding seed labels"
exit 0
