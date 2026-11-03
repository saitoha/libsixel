#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify cpu weighted R256 refined exact seed labels.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" fhedt/cpu_weighted_sparse_r256_refine || {
    echo "not ok 1 - cpu weighted R256 refined exact seed labels"
    exit 0
}
echo "ok 1 - cpu weighted R256 refined exact seed labels"
exit 0
