#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame extended oklab linear.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0006_extended_coordinates" 1 inverse || {
    echo "not ok 1 - frame extended oklab linear"
    exit 0
}
echo "ok 1 - frame extended oklab linear"
exit 0
