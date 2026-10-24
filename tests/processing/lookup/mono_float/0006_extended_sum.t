#!/bin/sh
# Verify float monochrome lookup: extended sum.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_ENCODER_THREADING_POLICY=greedy SIXEL_ENCODER_THREADING_GLOBAL_BUDGET=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" lookup/mono_float/0006_extended_sum || {
    echo "not ok 1 - mono lookup extended sum"
    exit 0
}

echo "ok 1 - mono lookup extended sum"
exit 0
