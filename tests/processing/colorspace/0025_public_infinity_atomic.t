#!/bin/sh
# Verify public colorspace infinity atomic.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "12" || {
    echo "not ok 1 - public colorspace infinity atomic"
    exit 0
}
echo "ok 1 - public colorspace infinity atomic"
exit 0
