#!/bin/sh
# Policy: docs/threading/policy-modes.md
# Policy: docs/cli/threading-policy.md
# Verify threading modes, budget precedence and explicit stage overrides.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "threadpool/0003_thread_policy" || {
    echo "not ok 1 - threading policy contract"
    exit 0
}
echo "ok 1 - threading policy contract"
exit 0
