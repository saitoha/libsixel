#!/bin/sh
# Verify native monochrome: inverted din99d.
# Policy: docs/encoder/palette-construction/fixed-palettes.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" quant/palette/mono_float/0010_inverted_din99d || {
    echo "not ok 1 - mono inverted din99d"
    exit 0
}

echo "ok 1 - mono inverted din99d"
exit 0
