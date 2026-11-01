#!/bin/sh
# Verify binding allocation failure.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0024_binding_allocation_failure" || {
    echo "not ok 1 - binding allocation failure"
    exit 0
}
echo "ok 1 - binding allocation failure"
exit 0
