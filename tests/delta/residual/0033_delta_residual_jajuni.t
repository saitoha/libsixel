#!/bin/sh
# Verify delta residual jajuni.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0008_delta_residual_jajuni || {
    echo "not ok 1 - delta residual jajuni"
    exit 0
}
echo "ok 1 - delta residual jajuni"
exit 0
