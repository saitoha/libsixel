#!/bin/sh
# A stale resolved identity leaves history unchanged.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/retained/0040_advance_stale_identity" || {
    echo 'not ok 1 - A stale resolved identity leaves history unchanged.'
    exit 0
}
echo 'ok 1 - A stale resolved identity leaves history unchanged.'
exit 0
