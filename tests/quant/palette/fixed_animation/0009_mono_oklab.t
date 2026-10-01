#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify fixed animation: mono oklab.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/fixed_animation/0009_mono_oklab || {
    echo "not ok 1 - fixed animation mono oklab"
    exit 0
}

echo "ok 1 - fixed animation mono oklab"
exit 0
