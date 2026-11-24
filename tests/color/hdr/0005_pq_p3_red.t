#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify pq p3 red against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0005_pq_p3_red || {
    printf '\n%s\n' 'not ok 1 - pq_p3_red'
    exit 0
}
printf '\n%s\n' 'ok 1 - pq_p3_red'
exit 0
