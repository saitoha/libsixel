#!/bin/sh
# DIN99d coordinates retain P3 red, green and a mixed color.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0019_p3_din99d || {
    echo "not ok 1 - high-color p3 din99d"
    exit 0
}
echo "ok 1 - high-color p3 din99d"
exit 0
