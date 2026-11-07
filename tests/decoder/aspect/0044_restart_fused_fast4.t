#!/bin/sh
# Verify restart fused fast4.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0044_restart_fused_fast4" || {
    echo "not ok 1 - restart fused fast4"
    exit 0
}
echo "ok 1 - restart fused fast4"
exit 0
