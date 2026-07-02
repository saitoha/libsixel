#!/bin/sh
# Verify sparse parallel row chunks match serial decode.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0017_decoder_parallel_sparse_split_matches_serial" || {
    echo "not ok 1 - 0017_decoder_parallel_sparse_split_matches_serial"
    exit 0
}

echo "ok 1 - 0017_decoder_parallel_sparse_split_matches_serial"
exit 0
