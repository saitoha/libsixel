#!/bin/sh
# Policy: docs/threading/policy-modes.md
# Policy: docs/threading/encoder.md
# Verify playback admission enforces capacity.
set -eux

echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0021_playback_admission"
result=$?
set -e
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 - playback admission enforces capacity # SKIP thread support disabled"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - playback admission enforces capacity"
    exit 0
}
printf '\n%s\n' "ok 1 - playback admission enforces capacity"
exit 0
