#!/bin/sh
# Verify b4 latest latest session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0019_b4_latest_latest_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b4 latest latest session"
    exit 0
}
echo "ok 1 - b4 latest latest session"
exit 0
