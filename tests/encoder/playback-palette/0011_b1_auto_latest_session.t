#!/bin/sh
# Verify b1 auto latest session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0011_b1_auto_latest_session" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - b1 auto latest session"
    exit 0
}
echo "ok 1 - b1 auto latest session"
exit 0
