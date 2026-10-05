#!/bin/sh
# Verify delta workspace mask.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0094_delta_workspace_mask || {
    echo "not ok 1 - delta workspace mask"
    exit 0
}
echo "ok 1 - delta workspace mask"
exit 0
