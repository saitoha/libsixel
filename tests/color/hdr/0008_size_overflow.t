#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify size overflow against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0008_size_overflow || {
    echo 'not ok 1 - size_overflow'
    exit 0
}
echo 'ok 1 - size_overflow'
exit 0
