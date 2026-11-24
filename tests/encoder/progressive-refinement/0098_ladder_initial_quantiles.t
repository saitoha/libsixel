#!/bin/sh
# Verify ladder initial quantiles.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0130_ladder_initial_quantiles" || {
    printf '\n%s\n' "not ok 1 - ladder initial quantiles"
    exit 0
}
printf '\n%s\n' "ok 1 - ladder initial quantiles"
exit 0
