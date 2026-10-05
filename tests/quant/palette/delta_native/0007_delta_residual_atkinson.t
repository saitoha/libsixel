#!/bin/sh
# Verify delta residual atkinson.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0007_delta_residual_atkinson || {
    echo "not ok 1 - delta residual atkinson"
    exit 0
}
echo "ok 1 - delta residual atkinson"
exit 0
