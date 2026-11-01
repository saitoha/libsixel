#!/bin/sh
# The body encoder refuses an incomplete producer at finish.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/indexed/0042_core_early_finish" || {
    echo 'not ok 1 - core_early_finish'
    exit 0
}
echo 'ok 1 - core_early_finish'
exit 0
