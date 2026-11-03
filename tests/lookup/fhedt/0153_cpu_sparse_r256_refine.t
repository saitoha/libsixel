#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify cpu r256 refined exact labels and tie order.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" fhedt/cpu_sparse_r256_refine || {
    echo "not ok 1 - cpu r256 refined exact labels and tie order"
    exit 0
}
echo "ok 1 - cpu r256 refined exact labels and tie order"
exit 0
