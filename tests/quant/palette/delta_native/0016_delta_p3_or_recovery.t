#!/bin/sh
# Verify delta p3 or rejection recovery.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0016_delta_p3_or_recovery || {
    echo "not ok 1 - delta p3 or rejection recovery"
    exit 0
}
echo "ok 1 - delta p3 or rejection recovery"
exit 0
