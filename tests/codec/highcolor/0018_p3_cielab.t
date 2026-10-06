#!/bin/sh
# CIELAB coordinates retain P3 red, green and a mixed color.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0018_p3_cielab || {
    echo "not ok 1 - high-color p3 cielab"
    exit 0
}
echo "ok 1 - high-color p3 cielab"
exit 0
