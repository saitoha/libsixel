#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify GPU application keeps the full CPU encode reservation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/allocation/0003_allocation_gpu" || {
    echo "not ok 1 - GPU application keeps the full CPU encode reservation"
    exit 0
}

echo "ok 1 - GPU application keeps the full CPU encode reservation"
exit 0
