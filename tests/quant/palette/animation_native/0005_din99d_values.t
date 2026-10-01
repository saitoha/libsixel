#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify generated animation: din99d values.
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    quant/palette/animation_native/0005_din99d_values || {
    echo "not ok 1 - animation din99d values"
    exit 0
}

echo "ok 1 - animation din99d values"
exit 0
