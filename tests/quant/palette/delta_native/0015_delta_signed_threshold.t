#!/bin/sh
# Verify delta signed threshold.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0015_delta_signed_threshold || {
    echo "not ok 1 - delta signed threshold"
    exit 0
}
echo "ok 1 - delta signed threshold"
exit 0
