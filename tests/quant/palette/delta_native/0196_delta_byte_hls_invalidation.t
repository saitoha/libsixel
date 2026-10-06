#!/bin/sh
# Generated HLS invalidation preserves exact painted history, compatible keeps and byte eligibility.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0196_delta_byte_hls_invalidation || {
    echo "not ok 1 - delta byte hls invalidation"
    exit 0
}
echo "ok 1 - delta byte hls invalidation"
exit 0
