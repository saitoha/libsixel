#!/bin/sh
# Sparse four-channel statistics stay within a 5 MiB budget.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/heckbert_policy/bucket_sparse_rgba >/dev/null || {
    echo "not ok 1 - bucket_sparse_rgba"
    exit 0
}

echo "ok 1 - bucket_sparse_rgba"
exit 0
