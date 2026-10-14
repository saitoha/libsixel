#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Require the parallel decoder to decline an OR-mode direct-color request.

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
