#!/bin/sh
# Verify float colorspace worker error.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float_fault" "3" || {
    echo "not ok 1 - float colorspace worker error"
    exit 0
}
echo "ok 1 - float colorspace worker error"
exit 0
