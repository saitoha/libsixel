#!/bin/sh
# Invalidation permits a different publication contract.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0038_invalidate_changes_contract" || {
    echo 'not ok 1 - Invalidation permits a different publication contract.'
    exit 0
}
echo 'ok 1 - Invalidation permits a different publication contract.'
exit 0
