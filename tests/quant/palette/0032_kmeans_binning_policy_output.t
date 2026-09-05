#!/bin/sh
# Verify pre-extraction Kmeans hard, soft, and feedback palette bytes.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0032_kmeans_binning_policy_output" || {
    echo "not ok 1 - Kmeans binning policy palette output"
    exit 0
}

echo "ok 1 - Kmeans binning policy palette output"
exit 0
