#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify pipeline and staged allocations respect their phase budgets.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/allocation/0001_allocation_slots" || {
    echo "not ok 1 - pipeline and staged allocations respect their phase budgets"
    exit 0
}

echo "ok 1 - pipeline and staged allocations respect their phase budgets"
exit 0
