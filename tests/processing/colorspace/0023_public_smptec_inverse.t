#!/bin/sh
# Verify public colorspace smptec inverse.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "10" || {
    echo "not ok 1 - public colorspace smptec inverse"
    exit 0
}
echo "ok 1 - public colorspace smptec inverse"
exit 0
