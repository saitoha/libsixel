#!/bin/sh
# Verify precision-policy=8bit rounds a float clustering palette to bytes.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0011_byte_request || {
    echo "not ok 1 - byte request"
    exit 0
}
echo "ok 1 - byte request"
exit 0
