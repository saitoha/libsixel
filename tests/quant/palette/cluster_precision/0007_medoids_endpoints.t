#!/bin/sh
# Verify float clustering medoids endpoints.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0007_medoids_endpoints || {
    echo "not ok 1 - medoids endpoints"
    exit 0
}
echo "ok 1 - medoids endpoints"
exit 0
