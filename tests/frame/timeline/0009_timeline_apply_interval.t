#!/bin/sh
# Policy: docs/cli/diagnostics.md
# Cover the common application interval at serial and parallel budgets.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0009_timeline_apply_interval" || {
    printf '\n%s\n' "not ok 1 - palette application interval contract changed"
    exit 0
}

printf '\n%s\n' "ok 1 - palette application records one interval at 1, 2, and 4 threads"
exit 0
