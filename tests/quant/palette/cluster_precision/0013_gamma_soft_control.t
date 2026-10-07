#!/bin/sh
# Verify float clustering gamma soft control.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0013_gamma_soft_control || {
    echo "not ok 1 - gamma soft control"
    exit 0
}
echo "ok 1 - gamma soft control"
exit 0
