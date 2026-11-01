#!/bin/sh
# Verify public colorspace gamma linear.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "2" || {
    echo "not ok 1 - public colorspace gamma linear"
    exit 0
}
echo "ok 1 - public colorspace gamma linear"
exit 0
