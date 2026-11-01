#!/bin/sh
# Invalidation cannot race a prepared update.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0013_retained_busy_invalidate" || {
    echo 'not ok 1 - retained_busy_invalidate'
    exit 0
}
echo 'ok 1 - retained_busy_invalidate'
exit 0
