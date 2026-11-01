#!/bin/sh
# Independent byte HLS contexts rebuild comparison views after palette changes.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0198_delta_byte_hls_instances || {
    echo "not ok 1 - delta byte hls instances"
    exit 0
}
echo "ok 1 - delta byte hls instances"
exit 0
