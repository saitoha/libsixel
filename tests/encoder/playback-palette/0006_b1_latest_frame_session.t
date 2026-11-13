#!/bin/sh
# Verify b1 latest frame session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0006_b1_latest_frame_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b1 latest frame session"
    exit 0
}
echo "ok 1 - b1 latest frame session"
exit 0
