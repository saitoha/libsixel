#!/bin/sh
# Verify float clustering linear mean.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0001_linear_mean || {
    echo "not ok 1 - linear mean"
    exit 0
}
echo "ok 1 - linear mean"
exit 0
