#!/bin/sh
# Byte gamma identity retains the byte palette and original channel percentages.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0001_byte_identity || {
    echo "not ok 1 - high-color byte identity"
    exit 0
}
echo "ok 1 - high-color byte identity"
exit 0
