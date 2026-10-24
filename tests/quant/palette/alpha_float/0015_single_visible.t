#!/bin/sh
# Verify native alpha: single visible.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0015_single_visible || {
    echo "not ok 1 - native alpha single visible"
    exit 0
}

echo "ok 1 - native alpha single visible"
exit 0
