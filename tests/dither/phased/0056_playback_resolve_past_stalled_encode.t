#!/bin/sh
# Policy: docs/threading/animation.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "phased/playback_resolve_past_stalled_encode" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP Metal device unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - resolve past stalled encode'
    exit 0
}
echo 'ok 1 - resolve past stalled encode'
exit 0
