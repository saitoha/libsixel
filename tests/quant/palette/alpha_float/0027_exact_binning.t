#!/bin/sh
# Verify native alpha: exact binning.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0027_exact_binning || {
    echo "not ok 1 - native alpha exact binning"
    exit 0
}

echo "ok 1 - native alpha exact binning"
exit 0
