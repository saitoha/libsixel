#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify pq bt2020 red against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0004_pq_bt2020_red || {
    echo 'not ok 1 - pq_bt2020_red'
    exit 0
}
echo 'ok 1 - pq_bt2020_red'
exit 0
