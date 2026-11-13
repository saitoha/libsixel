#!/bin/sh
# Verify b4 auto frame session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0022_b4_auto_frame_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b4 auto frame session"
    exit 0
}
echo "ok 1 - b4 auto frame session"
exit 0
