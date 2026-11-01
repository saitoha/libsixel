#!/bin/sh
# smptec output thresholds use destination units and return working residuals.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0131_delta_smptec_output_threshold || {
    echo "not ok 1 - delta smptec output threshold"
    exit 0
}
echo "ok 1 - delta smptec output threshold"
exit 0
