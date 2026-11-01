#!/bin/sh
# Verify delta p3 cache failure.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0110_delta_p3_cache_failure || {
    echo "not ok 1 - delta p3 cache failure"
    exit 0
}
echo "ok 1 - delta p3 cache failure"
exit 0
