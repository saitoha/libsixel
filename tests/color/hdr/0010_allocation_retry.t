#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify allocation retry against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0010_allocation_retry || {
    echo 'not ok 1 - allocation_retry'
    exit 0
}
echo 'ok 1 - allocation_retry'
exit 0
