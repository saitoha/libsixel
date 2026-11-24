#!/bin/sh
# Verify exact G8 normalization and RGB888 scale output.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "scale/0004_g8_normalized_rgb888" || {
    printf '\n%s\n' "not ok 1 - G8 scaling writes exact normalized RGB888 output"
    exit 0
}

printf '\n%s\n' "ok 1 - G8 scaling writes exact normalized RGB888 output"
exit 0
