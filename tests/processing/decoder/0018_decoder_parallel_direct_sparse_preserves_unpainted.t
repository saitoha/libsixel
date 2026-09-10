#!/bin/sh
# Policy: docs/functionality/decoding-pipeline.md
# Verify direct parallel sparse paint leaves untouched cells unchanged.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0018_decoder_parallel_direct_sparse_preserves_unpainted" || {
    echo "not ok 1 - 0018_decoder_parallel_direct_sparse_preserves_unpainted"
    exit 0
}

echo "ok 1 - 0018_decoder_parallel_direct_sparse_preserves_unpainted"
exit 0
