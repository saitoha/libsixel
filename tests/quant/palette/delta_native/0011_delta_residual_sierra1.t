#!/bin/sh
# Verify delta residual sierra1.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0011_delta_residual_sierra1 || {
    echo "not ok 1 - delta residual sierra1"
    exit 0
}
echo "ok 1 - delta residual sierra1"
exit 0
