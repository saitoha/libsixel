#!/bin/sh
# playback direct t1 d0 q1.
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-22
# Policy: docs/threading/encoder.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread support disabled"
    exit 0
}

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0020_playback_direct_equivalence 0 1 1 0 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP thread support disabled"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - playback direct t1 d0 q1"
    exit 0
}
echo "ok 1 - playback direct t1 d0 q1"
exit 0
