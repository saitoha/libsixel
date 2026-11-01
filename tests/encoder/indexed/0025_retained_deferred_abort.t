#!/bin/sh
# The last resource user safely drains an abandoned update.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0025_retained_deferred_abort" || {
    echo 'not ok 1 - retained_deferred_abort'
    exit 0
}
echo 'ok 1 - retained_deferred_abort'
exit 0
