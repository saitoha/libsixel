#!/bin/sh
# Verify b1 latest auto session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0008_b1_latest_auto_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b1 latest auto session"
    exit 0
}
echo "ok 1 - b1 latest auto session"
exit 0
