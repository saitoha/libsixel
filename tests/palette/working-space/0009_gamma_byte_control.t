#!/bin/sh
# Verify float clustering gamma byte control.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0009_gamma_byte_control || {
    echo "not ok 1 - gamma byte control"
    exit 0
}
echo "ok 1 - gamma byte control"
exit 0
