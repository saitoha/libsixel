#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Verify pq neutral against independent reference values.
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" hdr/0001_pq_neutral || {
    printf '\n%s\n' 'not ok 1 - pq_neutral'
    exit 0
}
printf '\n%s\n' 'ok 1 - pq_neutral'
exit 0
