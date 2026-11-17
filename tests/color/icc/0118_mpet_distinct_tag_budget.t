#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0118_mpet_distinct_tag_budget || {
    echo "not ok 1 - distinct MPE tag ranges consume separate payload budgets"
    exit 0
}

echo "ok 1 - distinct MPE tag ranges consume separate payload budgets"
exit 0
