#!/bin/sh
# Verify native alpha: kmedoids.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0012_kmedoids || {
    echo "not ok 1 - native alpha kmedoids"
    exit 0
}

echo "ok 1 - native alpha kmedoids"
exit 0
