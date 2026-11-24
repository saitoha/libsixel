#!/bin/sh
# Policy: docs/threading/animation.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lso2_gpu/latest_completed_own" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP GPU playback unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - latest completed own'
    exit 0
}
printf '\n%s\n' 'ok 1 - latest completed own'
exit 0
