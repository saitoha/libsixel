#!/bin/sh
# Verify restart parallel rgba.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0043_restart_parallel_rgba" || {
    echo "not ok 1 - restart parallel rgba"
    exit 0
}
echo "ok 1 - restart parallel rgba"
exit 0
