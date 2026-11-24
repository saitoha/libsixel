#!/bin/sh
# Verify query blue cpu refresh.
# Policy: docs/encoder/output/delta-encoding.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "delta/comparison/0050_query_blue_cpu_refresh" >/dev/null || {
    printf '\n%s\n' "not ok 1 - query blue cpu refresh"
    exit 0
}

printf '\n%s\n' "ok 1 - query blue cpu refresh"
exit 0
