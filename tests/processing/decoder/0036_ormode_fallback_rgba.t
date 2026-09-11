#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Check all final pixels through the public decoder entry point.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0036_ormode_fallback_rgba" || {
    echo "not ok 1 - ormode_fallback_rgba"
    exit 0
}

echo "ok 1 - ormode_fallback_rgba"
exit 0
