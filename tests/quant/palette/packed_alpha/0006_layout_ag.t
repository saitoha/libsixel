#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: layout ag.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0006_layout_ag || {
    echo "not ok 1 - packed alpha layout ag"
    exit 0
}

echo "ok 1 - packed alpha layout ag"
exit 0
