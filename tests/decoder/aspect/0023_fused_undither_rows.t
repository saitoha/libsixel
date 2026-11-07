#!/bin/sh
# Verify fused undither rows.
set -eux
echo "1..1"
set -v
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0023_fused_undither_rows" || {
    echo "not ok 1 - fused undither rows"
    exit 0
}
SIXEL_DECODER_THREADING_POLICY=greedy SIXEL_DECODER_THREADING_GLOBAL_BUDGET=2 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/aspect/0023_fused_undither_rows" || {
    echo "not ok 1 - fused undither rows"
    exit 0
}
echo "ok 1 - fused undither rows"
exit 0
