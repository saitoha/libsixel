#!/bin/sh
# Verify native alpha: p3 cielab.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_POLICY=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0009_p3_cielab || {
    echo "not ok 1 - native alpha p3 cielab"
    exit 0
}

echo "ok 1 - native alpha p3 cielab"
exit 0
