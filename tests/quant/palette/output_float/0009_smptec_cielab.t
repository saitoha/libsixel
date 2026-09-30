#!/bin/sh
# Verify smptec cielab native output.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/output_float/0009_smptec_cielab || {
    echo "not ok 1 - smptec cielab native output"
    exit 0
}

echo "ok 1 - smptec cielab native output"
exit 0
