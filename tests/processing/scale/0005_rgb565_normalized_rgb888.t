#!/bin/sh
# Verify exact RGB565 normalization and RGB888 scale output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0005_rgb565_normalized_rgb888" || {
    echo "not ok 1 - RGB565 scaling writes exact normalized RGB888 output"
    exit 0
}

echo "ok 1 - RGB565 scaling writes exact normalized RGB888 output"
exit 0
