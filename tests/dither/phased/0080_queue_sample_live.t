#!/bin/sh
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "phased/queue_sample_live" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP playback backend unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - queue sample live'
    exit 0
}
echo 'ok 1 - queue sample live'
exit 0
