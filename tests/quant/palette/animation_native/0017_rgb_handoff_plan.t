#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: rgb handoff borrow.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0017_rgb_handoff_plan || {
    echo "not ok 1 - animation rgb handoff borrow"
    exit 0
}

echo "ok 1 - animation rgb handoff borrow"
exit 0
