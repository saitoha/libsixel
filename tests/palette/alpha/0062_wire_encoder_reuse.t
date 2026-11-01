#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: wire encoder reuse.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0028_wire_encoder_reuse || {
    echo "not ok 1 - packed alpha wire encoder reuse"
    exit 0
}

echo "ok 1 - packed alpha wire encoder reuse"
exit 0
