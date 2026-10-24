#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: 8bit composes fractions in float, then applies bytes.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0032_wire_fractional_byte_request || {
    echo "not ok 1 - packed alpha wire fractional byte request"
    exit 0
}

echo "ok 1 - packed alpha wire fractional byte request"
exit 0
