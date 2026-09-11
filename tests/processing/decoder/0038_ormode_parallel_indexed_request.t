#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Require the internal parallel indexed request to complete.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0038_ormode_parallel_indexed_request" || {
    echo "not ok 1 - ormode_parallel_indexed_request"
    exit 0
}

echo "ok 1 - ormode_parallel_indexed_request"
exit 0
