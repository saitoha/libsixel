#!/bin/sh
# Extended linear coordinates retain P3 red, green and a mixed color.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0016_p3_linear || {
    echo "not ok 1 - high-color p3 linear"
    exit 0
}
echo "ok 1 - high-color p3 linear"
exit 0
