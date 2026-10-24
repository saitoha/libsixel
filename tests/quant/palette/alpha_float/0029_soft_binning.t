#!/bin/sh
# Verify native alpha: soft binning.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0029_soft_binning || {
    echo "not ok 1 - native alpha soft binning"
    exit 0
}

echo "ok 1 - native alpha soft binning"
exit 0
