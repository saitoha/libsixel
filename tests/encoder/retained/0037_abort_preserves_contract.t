#!/bin/sh
# Abort does not change the epoch publication contract.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0037_abort_preserves_contract" || {
    echo 'not ok 1 - Abort does not change the epoch publication contract.'
    exit 0
}
echo 'ok 1 - Abort does not change the epoch publication contract.'
exit 0
