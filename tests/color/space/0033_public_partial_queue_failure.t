#!/bin/sh
# Verify float colorspace partial queue failure.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v

# Fixed worker counts exercise the same paths on small CI machines.
SIXEL_ENCODER_THREADING_POLICY=greedy
export SIXEL_ENCODER_THREADING_POLICY

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float_fault" "1" || {
    echo "not ok 1 - float colorspace partial queue failure"
    exit 0
}
echo "ok 1 - float colorspace partial queue failure"
exit 0
