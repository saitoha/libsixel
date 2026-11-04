#!/bin/sh
# Policy: docs/threading/overview.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "platform/threadpool/0015_failed_rejects_resume" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - failed rejects resume'
    exit 0
}
echo 'ok 1 - failed rejects resume'
exit 0
