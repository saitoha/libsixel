#!/bin/sh
# Verify public colorspace byte lut.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/public_float" "18" || {
    printf '\n%s\n' "not ok 1 - public colorspace byte lut"
    exit 0
}
printf '\n%s\n' "ok 1 - public colorspace byte lut"
exit 0
