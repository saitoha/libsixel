#!/bin/sh
# Verify ladder initial quantiles.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "progressive/0130_ladder_initial_quantiles" || {
    echo "not ok 1 - ladder initial quantiles"
    exit 0
}
echo "ok 1 - ladder initial quantiles"
exit 0
