#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Policy: docs/threading/policy-modes.md
# Verify independent object policies and their scoped execution.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "threadpool/0005_object_policy_isolation" || {
    echo "not ok 1 - object threading policy"
    exit 0
}
echo "ok 1 - object threading policy"
exit 0
