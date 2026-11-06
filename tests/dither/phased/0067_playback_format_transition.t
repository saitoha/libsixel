#!/bin/sh
# Policy: docs/threading/animation.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "phased/playback_format_transition" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU playback unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - playback format transition'
    exit 0
}
echo 'ok 1 - playback format transition'
exit 0
