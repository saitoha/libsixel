#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify automatic allocation respects method and budget boundaries.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/allocation/0009_allocation_automatic_policy" || {
    printf '\n%s\n' "not ok 1 - automatic allocation respects method and budget boundaries"
    exit 0
}

printf '\n%s\n' "ok 1 - automatic allocation respects method and budget boundaries"
exit 0
