#!/bin/sh
# Verify metric without mask.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0018_metric_without_mask || {
    echo "not ok 1 - metric without mask"
    exit 0
}
echo "ok 1 - metric without mask"
exit 0
