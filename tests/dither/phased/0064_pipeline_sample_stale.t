#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "phased/pipeline_sample_stale" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP GPU playback unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - pipeline sample stale'
    exit 0
}
echo 'ok 1 - pipeline sample stale'
exit 0
