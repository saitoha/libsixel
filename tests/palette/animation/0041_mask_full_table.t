#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mask full table.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0020_mask_full_table || {
    echo "not ok 1 - fixed animation mask full table"
    exit 0
}

echo "ok 1 - fixed animation mask full table"
exit 0
