#!/bin/sh
# Policy: docs/threading/decoder.md
# Keep the initial and most recent register across byte-span boundaries.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0091_parallel_inherited_register" || {
    echo "not ok 1 - parallel_inherited_register"
    exit 0
}

echo "ok 1 - parallel_inherited_register"
exit 0
