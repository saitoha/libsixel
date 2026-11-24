#!/bin/sh
# P3 HLS uses a float representative without RGB888 projection.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0022_p3_hls || {
    printf '\n%s\n' "not ok 1 - high-color p3 hls"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color p3 hls"
exit 0
