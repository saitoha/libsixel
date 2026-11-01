#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify explicit dither caps and geometry retain precedence.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/allocation/0002_allocation_controls" || {
    echo "not ok 1 - explicit dither caps and geometry retain precedence"
    exit 0
}

echo "ok 1 - explicit dither caps and geometry retain precedence"
exit 0
