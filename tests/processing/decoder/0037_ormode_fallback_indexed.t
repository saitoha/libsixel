#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Check serial OR output with a gather launch failure configured.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0037_ormode_fallback_indexed" || {
    echo "not ok 1 - ormode_fallback_indexed"
    exit 0
}

echo "ok 1 - ormode_fallback_indexed"
exit 0
