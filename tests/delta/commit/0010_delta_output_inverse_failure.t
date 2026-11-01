#!/bin/sh
# Nonfinite/overflow and unsupported output inverses leave the destination unchanged.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0141_delta_output_inverse_failure || {
    echo "not ok 1 - delta output inverse failure"
    exit 0
}
echo "ok 1 - delta output inverse failure"
exit 0
