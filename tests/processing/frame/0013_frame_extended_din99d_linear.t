#!/bin/sh
# Policy: docs/concepts/pixelformat-precision.md
# Verify frame extended din99d linear.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0006_extended_coordinates" 3 inverse || {
    echo "not ok 1 - frame extended din99d linear"
    exit 0
}
echo "ok 1 - frame extended din99d linear"
exit 0
