#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify cpu r256 omits refinement auxiliary storage.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" fhedt/cpu_r256_aux_memory || {
    echo "not ok 1 - cpu r256 omits refinement auxiliary storage"
    exit 0
}
echo "ok 1 - cpu r256 omits refinement auxiliary storage"
exit 0
