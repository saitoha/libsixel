#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify frame extended linear cielab.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0006_extended_coordinates" 2 forward || {
    echo "not ok 1 - frame extended linear cielab"
    exit 0
}
echo "ok 1 - frame extended linear cielab"
exit 0
