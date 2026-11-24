#!/bin/sh
# Byte sRGB input is converted to the P3 destination.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0020_p3_byte || {
    printf '\n%s\n' "not ok 1 - high-color p3 byte"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color p3 byte"
exit 0
