#!/bin/sh
# Policy: docs/threading/overview.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "platform/threadpool/0011_resume_rebinds_hooks" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - resume rebinds hooks'
    exit 0
}
echo 'ok 1 - resume rebinds hooks'
exit 0
