#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify unknown policies retain their existing pipeline reservation.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/allocation/0004_allocation_serial_policy" || {
    echo "not ok 1 - frame-global policies retain their existing pipeline reservation"
    exit 0
}

echo "ok 1 - frame-global policies retain their existing pipeline reservation"
exit 0
