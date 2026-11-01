#!/bin/sh
# Packed RGBA input converts to linear output without mutating the dither format.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0009_rgba_linear || {
    echo "not ok 1 - high-color rgba linear"
    exit 0
}
echo "ok 1 - high-color rgba linear"
exit 0
