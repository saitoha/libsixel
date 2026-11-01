#!/bin/sh
# Six chromatic sectors, gray and a double-rounding witness match independent HLS fields and decoded percentages.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0174_delta_hls_field_model || {
    echo "not ok 1 - delta hls field model"
    exit 0
}
echo "ok 1 - delta hls field model"
exit 0
