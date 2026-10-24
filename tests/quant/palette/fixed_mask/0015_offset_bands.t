#!/bin/sh
# Verify fixed palette mask: offset bands.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=4 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/fixed_mask/0015_offset_bands || {
    echo "not ok 1 - fixed palette mask offset bands"
    exit 0
}

echo "ok 1 - fixed palette mask offset bands"
exit 0
