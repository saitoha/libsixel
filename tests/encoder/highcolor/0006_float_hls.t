#!/bin/sh
# HLS output uses the float representative without intermediate byte rounding.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0006_float_hls || {
    echo "not ok 1 - high-color float hls"
    exit 0
}
echo "ok 1 - high-color float hls"
exit 0
