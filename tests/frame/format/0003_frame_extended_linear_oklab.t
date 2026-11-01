#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame extended linear oklab.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0006_extended_coordinates" 1 forward || {
    echo "not ok 1 - frame extended linear oklab"
    exit 0
}
echo "ok 1 - frame extended linear oklab"
exit 0
