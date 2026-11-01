#!/bin/sh
# Policy: docs/threading/encoder.md
# A blocked writer times out drain then drains after release
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0108_playback_drain_timeout || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - A blocked writer times out drain then drains after release"
    exit 0
}
echo "ok 1 - A blocked writer times out drain then drains after release"
exit 0
