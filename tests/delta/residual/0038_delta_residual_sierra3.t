#!/bin/sh
# Verify delta residual sierra3.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0013_delta_residual_sierra3 || {
    printf '\n%s\n' "not ok 1 - delta residual sierra3"
    exit 0
}
printf '\n%s\n' "ok 1 - delta residual sierra3"
exit 0
