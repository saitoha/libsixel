#!/bin/sh
# Verify delta byte control.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0002_delta_byte_control || {
    echo "not ok 1 - delta byte control"
    exit 0
}
echo "ok 1 - delta byte control"
exit 0
