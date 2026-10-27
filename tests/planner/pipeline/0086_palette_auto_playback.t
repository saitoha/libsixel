#!/bin/sh
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-22
# Verify adaptive palette playback with injected durations.
# Policy: docs/cli/threading-policy.md
# Build configuration: docs/build/meson.md
# Policy: docs/threading/policy-modes.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0086_palette_auto_playback || {
    echo "not ok 1 - palette auto playback"
    exit 0
}
echo "ok 1 - palette auto playback"
exit 0
