#!/bin/sh
# Verify public colorspace cielab forward.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "5" || {
    echo "not ok 1 - public colorspace cielab forward"
    exit 0
}
echo "ok 1 - public colorspace cielab forward"
exit 0
