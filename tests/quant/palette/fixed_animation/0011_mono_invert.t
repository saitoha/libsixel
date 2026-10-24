#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mono invert.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0011_mono_invert || {
    echo "not ok 1 - fixed animation mono invert"
    exit 0
}

echo "ok 1 - fixed animation mono invert"
exit 0
