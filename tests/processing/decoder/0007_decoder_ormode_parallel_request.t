#!/bin/sh
# Run the OR-mode parallel decoder request unit test.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0007_decoder_ormode_parallel_request" || {
    echo "not ok 1 - 0007_decoder_ormode_parallel_request"
    exit 0
}

echo "ok 1 - 0007_decoder_ormode_parallel_request"
exit 0
