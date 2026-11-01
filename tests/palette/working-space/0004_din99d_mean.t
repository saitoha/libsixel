#!/bin/sh
# Verify float clustering din99d mean.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/cluster_precision/0004_din99d_mean || {
    echo "not ok 1 - din99d mean"
    exit 0
}
echo "ok 1 - din99d mean"
exit 0
