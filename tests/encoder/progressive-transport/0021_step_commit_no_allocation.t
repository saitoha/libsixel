#!/bin/sh
# Verify step commit no allocation.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0089_step_commit_no_allocation" || {
    echo "not ok 1 - step commit no allocation"
    exit 0
}
echo "ok 1 - step commit no allocation"
exit 0
