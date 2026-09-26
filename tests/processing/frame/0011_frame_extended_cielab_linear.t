#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame extended cielab linear.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0006_extended_coordinates" 2 inverse || {
    echo "not ok 1 - frame extended cielab linear"
    exit 0
}
echo "ok 1 - frame extended cielab linear"
exit 0
