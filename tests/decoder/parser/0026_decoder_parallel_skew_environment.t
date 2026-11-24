#!/bin/sh
# Run the decoder parallel skew environment unit test.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0024_decoder_parallel_skew_environment" || {
    printf '\n%s\n' "not ok 1 - decoder parallel skew missed span planning"
    exit 0
}

printf '\n%s\n' "ok 1 - decoder parallel skew reaches span planning"
exit 0
