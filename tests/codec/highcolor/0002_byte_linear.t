#!/bin/sh
# Byte gamma input converts to linear destination values before key selection.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0002_byte_linear || {
    echo "not ok 1 - high-color byte linear"
    exit 0
}
echo "ok 1 - high-color byte linear"
exit 0
