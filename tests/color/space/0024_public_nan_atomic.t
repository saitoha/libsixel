#!/bin/sh
# Verify public colorspace nan atomic.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "11" || {
    printf '\n%s\n' "not ok 1 - public colorspace nan atomic"
    exit 0
}
printf '\n%s\n' "ok 1 - public colorspace nan atomic"
exit 0
