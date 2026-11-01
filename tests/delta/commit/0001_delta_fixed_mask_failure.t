#!/bin/sh
# Verify delta fixed mask failure.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0050_delta_fixed_mask_failure || {
    echo "not ok 1 - delta fixed mask failure"
    exit 0
}
echo "ok 1 - delta fixed mask failure"
exit 0
