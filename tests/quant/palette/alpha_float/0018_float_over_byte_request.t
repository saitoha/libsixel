#!/bin/sh
# Verify native alpha: 8bit applies float input through bytes.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/alpha_float/0018_float_over_byte_request || {
    echo "not ok 1 - native alpha float over byte request"
    exit 0
}

echo "ok 1 - native alpha float over byte request"
exit 0
