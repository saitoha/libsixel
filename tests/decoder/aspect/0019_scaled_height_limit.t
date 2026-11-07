#!/bin/sh
# Verify scaled height limit.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0019_scaled_height_limit" || {
    echo "not ok 1 - scaled height limit"
    exit 0
}
echo "ok 1 - scaled height limit"
exit 0
