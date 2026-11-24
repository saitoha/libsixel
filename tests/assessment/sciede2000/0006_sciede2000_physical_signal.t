#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "assessment/0044_sciede2000_physical_signal" || {
    printf '\n%s\n' "not ok 1 - sciede2000 physical signal"
    exit 0
}
printf '\n%s\n' "ok 1 - sciede2000 physical signal"
exit 0
