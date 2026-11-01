#!/bin/sh
# Verify native alpha: srgb gamma.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0001_srgb_gamma || {
    echo "not ok 1 - native alpha srgb gamma"
    exit 0
}

echo "ok 1 - native alpha srgb gamma"
exit 0
