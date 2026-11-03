#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify cpu unrefined grids omit auxiliary storage.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" fhedt/cpu_unrefined_aux_memory || {
    echo "not ok 1 - cpu unrefined grids omit auxiliary storage"
    exit 0
}
echo "ok 1 - cpu unrefined grids omit auxiliary storage"
exit 0
