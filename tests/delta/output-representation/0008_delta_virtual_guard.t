#!/bin/sh
# Reject bright delta history for a virtual DIN99d query.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0099_delta_virtual_guard || {
    echo "not ok 1 - virtual guard"
    exit 0
}
echo "ok 1 - virtual guard"
exit 0
