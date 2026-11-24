#!/bin/sh
# Verify ladder outlier descent.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0136_ladder_outlier_descent" || {
    printf '\n%s\n' "not ok 1 - ladder outlier descent"
    exit 0
}
printf '\n%s\n' "ok 1 - ladder outlier descent"
exit 0
