#!/bin/sh
# Verify delta float centroid.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0001_delta_float_centroid || {
    echo "not ok 1 - delta float centroid"
    exit 0
}
echo "ok 1 - delta float centroid"
exit 0
