#!/bin/sh
# Verify delta wire disabled.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0055_delta_wire_disabled || {
    echo "not ok 1 - delta wire disabled"
    exit 0
}
echo "ok 1 - delta wire disabled"
exit 0
