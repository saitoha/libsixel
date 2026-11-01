#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame extended linear gamma.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0006_extended_coordinates" 0 forward || {
    echo "not ok 1 - frame extended linear gamma"
    exit 0
}
echo "ok 1 - frame extended linear gamma"
exit 0
