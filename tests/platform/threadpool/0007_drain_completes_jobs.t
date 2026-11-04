#!/bin/sh
# Policy: docs/threading/overview.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "platform/threadpool/0007_drain_completes_jobs" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - drain completes jobs'
    exit 0
}
echo 'ok 1 - drain completes jobs'
exit 0
