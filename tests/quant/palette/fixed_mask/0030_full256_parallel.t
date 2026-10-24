#!/bin/sh
# Verify fixed palette mask: full256 parallel.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0030_full256_parallel || {
    echo "not ok 1 - fixed palette mask full256 parallel"
    exit 0
}

echo "ok 1 - fixed palette mask full256 parallel"
exit 0
