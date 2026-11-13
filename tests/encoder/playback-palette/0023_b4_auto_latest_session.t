#!/bin/sh
# Verify b4 auto latest session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0023_b4_auto_latest_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b4 auto latest session"
    exit 0
}
echo "ok 1 - b4 auto latest session"
exit 0
