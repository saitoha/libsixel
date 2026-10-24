#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Policy: docs/threading/policy-modes.md
# Verify independent object policies and their scoped execution.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is disabled"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "threadpool/0006_concurrent_object_policies" || {
    echo "not ok 1 - object threading policy"
    exit 0
}
echo "ok 1 - object threading policy"
exit 0
