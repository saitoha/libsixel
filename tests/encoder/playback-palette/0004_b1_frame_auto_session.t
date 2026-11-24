#!/bin/sh
# Verify b1 frame auto session.
# Policy: docs/cli/threading-policy.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/playback-palette/0004_b1_frame_auto_session" || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - b1 frame auto session"
    exit 0
}
printf '\n%s\n' "ok 1 - b1 frame auto session"
exit 0
