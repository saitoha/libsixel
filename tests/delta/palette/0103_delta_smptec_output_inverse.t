#!/bin/sh
# Independent smptec output inverse preserves extended coordinates.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0130_delta_smptec_output_inverse || {
    echo "not ok 1 - delta smptec output inverse"
    exit 0
}
echo "ok 1 - delta smptec output inverse"
exit 0
