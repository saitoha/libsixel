#!/bin/sh
# Verify float colorspace pool creation failure.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float_fault" "2" || {
    echo "not ok 1 - float colorspace pool creation failure"
    exit 0
}
echo "ok 1 - float colorspace pool creation failure"
exit 0
