#!/bin/sh
# Verify delta p3 threshold.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0106_delta_p3_threshold || {
    echo "not ok 1 - delta p3 threshold"
    exit 0
}
echo "ok 1 - delta p3 threshold"
exit 0
