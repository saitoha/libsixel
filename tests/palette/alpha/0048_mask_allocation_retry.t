#!/bin/sh
# Verify packed alpha: mask allocation retry.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0014_mask_allocation_retry || {
    echo "not ok 1 - packed alpha mask allocation retry"
    exit 0
}

echo "ok 1 - packed alpha mask allocation retry"
exit 0
