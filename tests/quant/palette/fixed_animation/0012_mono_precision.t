#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mono precision.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0012_mono_precision || {
    echo "not ok 1 - fixed animation mono precision"
    exit 0
}

echo "ok 1 - fixed animation mono precision"
exit 0
