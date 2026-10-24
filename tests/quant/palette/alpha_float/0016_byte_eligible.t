#!/bin/sh
# Verify native alpha: byte eligible.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0016_byte_eligible || {
    echo "not ok 1 - native alpha byte eligible"
    exit 0
}

echo "ok 1 - native alpha byte eligible"
exit 0
