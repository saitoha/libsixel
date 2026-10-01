#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify packed alpha: alpha disabled.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/packed_alpha/0008_alpha_disabled || {
    echo "not ok 1 - packed alpha alpha disabled"
    exit 0
}

echo "ok 1 - packed alpha alpha disabled"
exit 0
