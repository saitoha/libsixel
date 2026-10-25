#!/bin/sh
# Policy: docs/cli/threading-policy.md
# Verify playback palette precedence at execution and frame fallback bytes.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0026_playback_palette_policy || {
    echo "not ok 1 - playback palette precedence and frame fallback"
    exit 0
}
echo "ok 1 - playback palette precedence and frame fallback"
exit 0
