#!/bin/sh
# Compare serial and parallel decoder output at a DECGNL split boundary.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0001_decoder_parallel_split_after_newline" || {
    echo "not ok 1 - 0001_decoder_parallel_split_after_newline"
    exit 0
}

echo "ok 1 - 0001_decoder_parallel_split_after_newline"
exit 0
