#!/bin/sh
# Policy: docs/threading/decoder.md
# A decoder object reuses its parallel decode workers across calls.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0111_resident_workers" || {
    printf '\n%s\n' "not ok 1 - decoder objects reuse their decode workers"
    exit 0
}

printf '\n%s\n' "ok 1 - decoder objects reuse their decode workers"
exit 0
