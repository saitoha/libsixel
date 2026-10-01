#!/bin/sh
# Verify native alpha: kcenter.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0011_kcenter || {
    echo "not ok 1 - native alpha kcenter"
    exit 0
}

echo "ok 1 - native alpha kcenter"
exit 0
