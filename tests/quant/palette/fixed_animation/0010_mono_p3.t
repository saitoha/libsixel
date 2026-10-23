#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mono p3.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0010_mono_p3 || {
    echo "not ok 1 - fixed animation mono p3"
    exit 0
}

echo "ok 1 - fixed animation mono p3"
exit 0
