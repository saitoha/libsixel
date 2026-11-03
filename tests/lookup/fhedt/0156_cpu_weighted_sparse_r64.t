#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify cpu weighted sparse seed labels.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" fhedt/cpu_weighted_sparse_r64 || {
    echo "not ok 1 - cpu weighted sparse seed labels"
    exit 0
}
echo "ok 1 - cpu weighted sparse seed labels"
exit 0
