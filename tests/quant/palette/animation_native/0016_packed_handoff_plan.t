#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: packed handoff copy.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0016_packed_handoff_plan || {
    echo "not ok 1 - animation packed handoff copy"
    exit 0
}

echo "ok 1 - animation packed handoff copy"
exit 0
