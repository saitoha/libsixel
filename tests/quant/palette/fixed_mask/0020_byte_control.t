#!/bin/sh
# Verify fixed palette mask: byte control.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0020_byte_control || {
    echo "not ok 1 - fixed palette mask byte control"
    exit 0
}

echo "ok 1 - fixed palette mask byte control"
exit 0
