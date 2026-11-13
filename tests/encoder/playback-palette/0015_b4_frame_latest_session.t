#!/bin/sh
# Verify b4 frame latest session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0015_b4_frame_latest_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b4 frame latest session"
    exit 0
}
echo "ok 1 - b4 frame latest session"
exit 0
